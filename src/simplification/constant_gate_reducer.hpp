#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <memory>
#include <ranges>
#include <utility>
#include <vector>

#include "src/algo.hpp"
#include "src/common/csat_types.hpp"
#include "src/simplification/transformer_base.hpp"
#include "src/structures/assignment/vector_assignment.hpp"
#include "src/structures/circuit/gate_info.hpp"
#include "src/utility/converters.hpp"
#include "src/utility/logger.hpp"

namespace csat::simplification
{

/**
 * Transformer, that cleans the circuit from constant gates (like AND(x, NOT(x)) = false)
 *
 *    Before            |          After
 *
 * INPUT(0)             |       INPUT(1)
 * INPUT(1)             |       INPUT(2)
 * INPUT(2)             |       5 = OR(1, 2)
 * 3 = NOT(0)           |       OUTPUT(5)
 * 4 = AND(0, 3)        |
 * 5 = OR(1, 2, 4)      |
 * OUTPUT(5)            |
 *
 * Note that this algorithm requires ReduceNotComposition_  and RedundantGatesCleaner_  to be applied right after.
 *
 * @tparam CircuitT
 */
template<class CircuitT>
class ConstantGateReducer_ : public ITransformer<CircuitT>
{
  private:
    csat::Logger logger{"ConstantGateReducer"};

  public:
    /**
     * Applies ConstantGateReducer_ transformer to `circuit`
     * @param circuit -- circuit to transform.
     * @param encoder -- circuit encoder.
     * @return  circuit and encoder after transformation.
     */
    CircuitAndEncoder<CircuitT, std::string> transform(
        std::unique_ptr<CircuitT> circuit,
        std::unique_ptr<GateEncoder<std::string>> encoder)
    {
        logger.debug("START ConstantGateReducer");

        auto new_gate_name_prefix = (getUniqueId_() + "::new_gate_ConstantGateReducer@");

        static std::map<GateType, GateType> const xor_inverse_map = {
            {GateType::XOR,  GateType::NXOR},
            {GateType::NXOR, GateType::XOR }
        };

        csat::GateIdContainer gate_sorting(algo::TopSortAlgorithm<algo::DFSTopSort>::sorting(*circuit));

        size_t circuit_size = circuit->getNumberOfGates();
        GateInfoContainer gate_info(circuit_size);

        // Surjection of old gate ids to new gate ids.
        std::vector<GateId> old_to_new_gateId(circuit_size, SIZE_MAX);

        // Evaluate circuit.
        auto result_assignment = circuit->template evaluateCircuit<VectorAssignment<true>>(VectorAssignment<false>{});

        for (GateId gate_id : std::ranges::reverse_view(gate_sorting))
        {
            GateType gate_type = circuit->getGateType(gate_id);
            GateIdContainer operands{};

            // After partial circuit calculation, we need to leave only undefined gates.
            // Defined gates from the circuit must be removed, and users of these gates
            // must now use CONST_TRUE or CONST_FALSE as their operands.
            if (result_assignment->isUndefined(gate_id) || gate_type == GateType::CONST_TRUE ||
                gate_type == GateType::CONST_FALSE)
            {
                // if the operator is not symmetric, we cannot delete its operands even if they are constants
                if (!csat::utils::symmetricOperatorQ(gate_type))
                {
                    for (auto operand : circuit->getGateOperands(gate_id))
                    {
                        operands.push_back(getLink_(operand, old_to_new_gateId));
                    }
                }
                else
                {
                    static_assert(GateStateNumber == 3);
                    int64_t states_count[GateStateNumber]{0 /*False*/, 0 /*True*/, 0 /*Undefined*/};
                    for (auto operand : circuit->getGateOperands(gate_id))
                    {
                        operand            = getLink_(operand, old_to_new_gateId);
                        GateState op_state = result_assignment->getGateState(operand);
                        ++states_count[static_cast<uint8_t>(op_state)];

                        // We take only unknown operands.
                        // Because if we encounter non-significant operands, such as an operand with value TRUE in a
                        // gate of type AND, they can simply be skipped. And if the operand is significant, then the
                        // gate itself would have known value, for example, in a gate of type TRUE, there is an operand
                        // of type OR. So we will not consider it. As for XOR and NXOR, we simply skip operands with the
                        // value FALSE. And we count the number of operands with the value TRUE, then it will be
                        // necessary to change the gate type to the opposite.
                        if (op_state == GateState::UNDEFINED)
                        {
                            operands.push_back(operand);
                        }
                    }

                    // Change the gate to the opposite.
                    // Examples:
                    //   1. XOR(TRUE, y, z) = NXOR(y, z)
                    //   2. XOR(TRUE, TRUE, FALSE, y, z) = XOR(y, z)
                    //   3. XOR(FALSE, y, z) = XOR(y, z)
                    if ((gate_type == GateType::XOR || gate_type == GateType::NXOR) &&
                        (states_count[static_cast<uint8_t>(GateState::TRUE)] % 2 == 1))
                    {
                        gate_type = xor_inverse_map.at(gate_type);
                    }
                }

                // Prepare gate_info for current gate.
                gate_info.at(gate_id) = {gate_type, operands};

                // If, after the reduction of the assigned gates, current gate left with only one operand,
                // then all its users must be transferred either to its operand or to its negation.
                if (operands.size() == 1 && (gate_type == GateType::AND || gate_type == GateType::OR ||
                                             gate_type == GateType::XOR || gate_type == GateType::IFF))
                {
                    // Users of the current gate will refer to its operand.
                    old_to_new_gateId.at(gate_id) = operands.at(0);
                }
                else if (
                    operands.size() == 1 &&
                    (gate_type == GateType::NAND || gate_type == GateType::NOR || gate_type == GateType::NXOR))
                {
                    // Create NOT.
                    GateId const new_gate_id = encoder->encodeGate(getNewGateName_(new_gate_name_prefix, circuit_size));
                    assert(new_gate_id == circuit_size);
                    assert(new_gate_id == gate_info.size());

                    gate_info.emplace_back(GateType::NOT, GateIdContainer{operands.at(0)});
                    result_assignment->assign(new_gate_id, GateState::UNDEFINED);

                    // Users of the current gate will refer to the negation of its operand.
                    old_to_new_gateId.at(gate_id) = new_gate_id;
                    // The gate NOT must also refer to itself.
                    old_to_new_gateId.push_back(new_gate_id);

                    ++circuit_size;
                    assert(old_to_new_gateId.size() == circuit_size);
                }
                else if (gate_type == GateType::MUX)
                {
                    auto first_operand           = getLink_(circuit->getGateOperands(gate_id)[0], old_to_new_gateId);
                    GateState const mux_op_state = result_assignment->getGateState(first_operand);
                    if (mux_op_state == GateState::TRUE)
                    {
                        // Users of the current gate will refer to its third operand.
                        old_to_new_gateId.at(gate_id) =
                            getLink_(circuit->getGateOperands(gate_id)[2], old_to_new_gateId);
                    }
                    else if (mux_op_state == GateState::FALSE)
                    {
                        // Users of the current gate will refer to its second operand.
                        old_to_new_gateId.at(gate_id) =
                            getLink_(circuit->getGateOperands(gate_id)[1], old_to_new_gateId);
                    }
                    else
                    {
                        // Gate statue is unknown, we don't touch it
                        old_to_new_gateId.at(gate_id) = gate_id;
                    }
                }
                else
                {
                    // We do not touch gates like INPUT, CONST_FALSE, CONST_TRUE, NOT and if the gate has 2+ operands or
                    // we across a new type of gate.
                    old_to_new_gateId.at(gate_id) = gate_id;
                }
            }
            // We will replace the remaining gates with constants,
            // these gates will be without users and will be removed later by a `RedundantGatesCleaner_` transformer.
            else if (result_assignment->getGateState(gate_id) == GateState::TRUE)
            {
                gate_info.at(gate_id) = {GateType::CONST_TRUE, {}};
            }
            else if (result_assignment->getGateState(gate_id) == GateState::FALSE)
            {
                gate_info.at(gate_id) = {GateType::CONST_FALSE, {}};
            }
            else
            {
                logger.error("Unsupported gate state");
                std::abort();
            }
        }

        // Rebuild OUTPUT. If we know the value of the output gate,
        // replace it with a little trivial circuit-gadget.
        GateIdContainer new_output_gates{};
        new_output_gates.reserve(circuit->getOutputGates().size());
        for (GateId output_gate : circuit->getOutputGates())
        {
            if (result_assignment->isUndefined(output_gate))
            {
                new_output_gates.push_back(old_to_new_gateId.at(output_gate));
            }
            else
            {
                createMiniCircuit_(
                    gate_info,
                    *encoder,
                    new_output_gates,
                    new_gate_name_prefix,
                    circuit_size,
                    result_assignment->getGateState(output_gate));
            }
        }

        logger.debug("END ConstantGateReducer");

        return {
            std::make_unique<CircuitT>(std::move(gate_info), std::move(new_output_gates)),
            std::make_unique<GateEncoder<std::string>>(*encoder)};
    };

  private:
    /**
     * Give a link either to the gate itself, or to the gate where the users of the gate should refer.
     * @param gate_id gate's id for which you need to find a link
     * @param old_to_new_gateId -- Surjection of old gate ids to new gate ids. For using
     *                             the final (after transformation) operand in the gate (= rehung)
     * @return gate's id like a link
     **/
    GateId getLink_(GateId gate_id, std::vector<GateId> const& old_to_new_gateId)
    {
        if (old_to_new_gateId.at(gate_id) != SIZE_MAX)
        {
            return old_to_new_gateId.at(gate_id);
        }
        return gate_id;
    }

    /**
     * Create a subcircuit whose response will be the output (created instead of the outputs
     * already existing in the original circuit, the value of which became known after the transformation)
     * @param gate_info -- container of the new (transformer-modified) circuit
     * @param encoder -- encoder of the new circuit
     * @param new_output_gates -- outputs in the new circuit
     * @param new_gate_name_prefix -- prefix for new gates
     * @param circuit_size -- new circuit size
     * @param gate_state -- constant true or false instead of which it is necessary to create a subcircuit
     * @return None. All transformations occur by changing the input data parameters
     **/
    void createMiniCircuit_(
        GateInfoContainer& gate_info,
        GateEncoder<std::string>& encoder,
        GateIdContainer& new_output_gates,
        std::string const& new_gate_name_prefix,
        GateId& circuit_size,
        GateState gate_state)
    {
        GateId gate_id_input = SIZE_MAX;
        for (GateId gate_id = 0; gate_id < gate_info.size(); ++gate_id)
        {
            if (gate_info.at(gate_id).getType() == GateType::INPUT)
            {
                gate_id_input = gate_id;
                break;
            }
        }
        assert(gate_id_input != SIZE_MAX);

        gate_info.reserve(circuit_size + 2);

        // We take first found already existing input to enforce connection
        // between original circuit gates and a newly built constant gadget.
        GateId const left   = gate_id_input;
        GateId const right  = circuit_size;
        GateId const output = circuit_size + 1;

        // Changing reference.
        circuit_size += 2;

        encoder.encodeGate(getNewGateName_(new_gate_name_prefix, right));
        gate_info.emplace_back(GateType::NOT, GateIdContainer{left});

        encoder.encodeGate(getNewGateName_(new_gate_name_prefix, output));
        if (gate_state == GateState::TRUE)
        {
            gate_info.emplace_back(GateType::OR, GateIdContainer{left, right});
        }
        else
        {
            gate_info.emplace_back(GateType::AND, GateIdContainer{left, right});
        }

        // Add recently build output to vector of new output gates.
        new_output_gates.push_back(output);
    }
};

}  // namespace csat::simplification

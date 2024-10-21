#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <ranges>
#include <utility>
#include <vector>

#include "src/algo.hpp"
#include "src/common/csat_types.hpp"
#include "src/simplification/transformer_base.hpp"
#include "src/structures/circuit/gate_info.hpp"
#include "src/utility/converters.hpp"
#include "src/utility/logger.hpp"

namespace csat::simplification
{

/**
 * Transformer, that cleans the circuit from gates with the same operands. For example:
 *
 *    Before            |          After
 *
 * INPUT(0)             |       INPUT(0)
 * INPUT(1)             |       INPUT(1)
 * 2 = AND(0, 0)        |       2 = AND(0, 1)
 * 3 = AND(1, 2)        |       OUTPUT(2)
 * OUTPUT(3)            |
 *
 * After the algorithm, there may be gates that have 1 or 0 operands. The next algorithm (ConstantGateReducer_) will
 * also make its reductions. And after RedundantGatesCleaner_ will remove all such gates from the circuit.
 *
 * Note that this algorithm requires ConstantGateReducer_, ReduceNotComposition_  and RedundantGatesCleaner_ to be
 * applied right after.
 *
 * @tparam CircuitT
 */
template<class CircuitT>
class DuplicateOperandsCleaner_ : public ITransformer<CircuitT>
{
  private:
    csat::Logger logger{"DuplicateOperandsCleaner"};
    size_t id_const_true  = SIZE_MAX;
    size_t id_const_false = SIZE_MAX;

  public:
    /**
     * Applies DuplicateOperandsCleaner_ transformer to `circuit`
     * @param circuit -- circuit to transform.
     * @param encoder -- circuit encoder.
     * @return  circuit and encoder after transformation.
     */
    CircuitAndEncoder<CircuitT, std::string> transform(
        std::unique_ptr<CircuitT> circuit,
        std::unique_ptr<GateEncoder<std::string>> encoder)
    {
        logger.debug("START DuplicateOperandsCleaner");

        auto new_gate_name_prefix = (getUniqueId_() + "::new_gate_DuplicateOperandsCleaner@");

        csat::GateIdContainer gate_sorting(algo::TopSortAlgorithm<algo::DFSTopSort>::sorting(*circuit));

        size_t circuit_size = circuit->getNumberOfGates();
        GateInfoContainer gate_info(circuit_size);

        // Surjection of old gate ids to new gate ids.
        GateIdContainer old_to_new_gateId(circuit_size, SIZE_MAX);

        // For XOR and NXOR, when we have to replace two opposite operands with CONST_TRUE
        // Example: XOR(x, NOT(x), y, z) = XOR(CONST_TRUE, y, z).
        bool rebuild_gate = false;

        // Prepare auxiliary const TRUE and FALSE gates.
        id_const_true = encoder->encodeGate(getNewGateName_(new_gate_name_prefix, "CONST_TRUE"));
        gate_info.emplace_back(GateType::CONST_TRUE, GateIdContainer{});
        old_to_new_gateId.push_back(id_const_true);
        ++circuit_size;

        id_const_false = encoder->encodeGate(getNewGateName_(new_gate_name_prefix, "CONST_FALSE"));
        gate_info.emplace_back(GateType::CONST_FALSE, GateIdContainer{});
        old_to_new_gateId.push_back(id_const_false);
        ++circuit_size;

        // Rebuild circuit.
        for (auto gate_id : std::ranges::reverse_view(gate_sorting))
        {
            // First of all, we will determine the correct gate operands by filling `old_to_new_gateId`.
            //      INPUT(0)        |   In this case gate 2 must contain input zero as an operand instread
            //      1 = AND(0, 0)   |   of gate 1. So the information we need in the `old_to_new_gateId`
            //      2 = NOT(1)      |   will be {1: 0}. This means that all users of gate 1 must now use
            //      OTPUT(2)        |   gate 0.

            GateType gate_type = circuit->getGateType(gate_id);

            // Count the occurrence of operands in the gate.
            std::map<GateId, size_t> map_count_operands = transformOperands_(*circuit, gate_id, old_to_new_gateId);

            if (gate_type == GateType::AND || gate_type == GateType::NAND || gate_type == GateType::OR ||
                gate_type == GateType::NOR || gate_type == GateType::XOR || gate_type == GateType::NXOR)
            {
                // If the gate consists of one repeating operand,
                // then all its users must be transferred either
                // to this operand or to its negation.
                if (map_count_operands.size() == 1)
                {
                    // No need to use getLink_. The map already has the correct (rehung) gates.
                    GateId const unique_operand = map_count_operands.begin()->first;
                    if (gate_type == GateType::AND || gate_type == GateType::OR || gate_type == GateType::XOR)
                    {
                        // Users of the current gate will refer to its operand.
                        old_to_new_gateId.at(gate_id) = unique_operand;
                    }
                    else
                    {
                        // Create NOT.
                        GateId const new_gate_id =
                            encoder->encodeGate(getNewGateName_(new_gate_name_prefix, circuit_size));
                        gate_info.emplace_back(GateType::NOT, GateIdContainer{unique_operand});
                        assert(new_gate_id == circuit_size);
                        assert(gate_info.size() - 1 == circuit_size);
                        ++circuit_size;

                        // Users of the current gate will refer to the negation of its operand.
                        old_to_new_gateId.at(gate_id) = new_gate_id;
                        // The gate NOT must also refer to itself.
                        old_to_new_gateId.push_back(new_gate_id);
                    }
                }
                else if (map_count_operands.empty())
                {
                    // If, as a result of counting of the operands, an empty map is obtained
                    // (it can be in gates of type XOR or NXOR), then we know their assignment.
                    if (gate_type == GateType::XOR)
                    {
                        old_to_new_gateId.at(gate_id) = id_const_false;
                    }
                    else
                    {
                        old_to_new_gateId.at(gate_id) = id_const_true;
                    }
                }
                else
                {
                    // If after counting there are 2+ operands left, try to find the opposite operands.
                    // If gates are found to have opposite operands, then all their users will have to use
                    // constants (CONST_TRUE, CONST_FALSE) as their operands instead of these gates.
                    bool flag = areThereOppositeOperands_(gate_info, map_count_operands);
                    if (flag && (gate_type == GateType::AND || gate_type == GateType::NOR))
                    {
                        old_to_new_gateId.at(gate_id) = id_const_false;
                    }
                    else if (flag && (gate_type == GateType::NAND || gate_type == GateType::OR))
                    {
                        old_to_new_gateId.at(gate_id) = id_const_true;
                    }
                    else if (flag)
                    {
                        // If gates like XOR or NXOR have opposite operands, we must completely rebuild it.
                        rebuild_gate                  = true;
                        old_to_new_gateId.at(gate_id) = gate_id;
                    }
                    else
                    {
                        // If there are more than 1 operands and there are no opposites among them,
                        // user links must remain the same.
                        old_to_new_gateId.at(gate_id) = gate_id;
                    }
                }
            }
            else
            {
                // We don't touch another gates.
                old_to_new_gateId.at(gate_id) = gate_id;
            }

            // The second step. Let's reassemble the operands, knowing that all the reductions are already
            // taken into account in the map.

            GateIdContainer operands{};

            // Rebuild the gate if necessary (only XOR or NXOR).
            if (rebuild_gate)
            {
                rebuild_gate = false;

                operands = rebuildXORAndNXOR_(gate_info, map_count_operands);

                // If all operands had an opposite pair, the gate may end up with one operand.
                if (operands.size() == 1)
                {
                    if (gate_type == GateType::XOR)
                    {
                        // Users of the current gate will refer to its operand.
                        old_to_new_gateId.at(gate_id) = operands.at(0);
                    }
                    else  // NXOR
                    {
                        // Create NOT.
                        GateId const new_gate_id =
                            encoder->encodeGate(getNewGateName_(new_gate_name_prefix, circuit_size));
                        assert(new_gate_id == circuit_size);
                        gate_info.emplace_back(GateType::NOT, GateIdContainer{operands.at(0)});

                        // Users of the current gate will refer to the negation of its operand.
                        old_to_new_gateId.at(gate_id) = new_gate_id;
                        // The gate NOT must also refer to itself.
                        old_to_new_gateId.push_back(new_gate_id);

                        ++circuit_size;
                    }
                }
                else if (operands.empty())
                {
                    if (gate_type == GateType::XOR)
                    {
                        old_to_new_gateId.at(gate_id) = id_const_false;
                    }
                    else
                    {
                        old_to_new_gateId.at(gate_id) = id_const_true;
                    }
                }
                // If the gate is left with 2+ operands, then it is considered syntactically correct
                // and nothing more needs to be done with it and its users.
            }
            else
            {
                // If possible, we will remove all unnecessary operands using data from map_count_operands.
                if (csat::utils::symmetricOperatorQ(gate_type))
                {
                    for (auto [operand, value] : map_count_operands)
                    {
                        for (size_t num_operands = 0; num_operands < value; ++num_operands)
                        {
                            operands.push_back(operand);
                        }
                    }
                }
                else
                {
                    for (GateId operand : circuit->getGateOperands(gate_id))
                    {
                        operands.push_back(getLink_(operand, old_to_new_gateId));
                    }
                }
            }

            // Construct the gate.
            gate_info.at(gate_id) = {gate_type, operands};
        }

        // Rebuild OUTPUT.
        GateIdContainer new_output_gates{};
        new_output_gates.reserve(circuit->getOutputGates().size());
        for (GateId output_gate : circuit->getOutputGates())
        {
            new_output_gates.push_back(old_to_new_gateId.at(output_gate));
        }

        logger.debug("END DuplicateOperandsCleaner");

        return {
            std::make_unique<CircuitT>(std::move(gate_info), std::move(new_output_gates)),
            std::make_unique<GateEncoder<std::string>>(*encoder)};
    };

  private:
    /**
     * Counts how many operands are in the gate and which ones specifically,
     * and also reduces them if possible
     * @param circuit -- our circuit
     * @param gate_id -- the gate ID for which operands need to be calculated
     * @param old_to_new_gateId -- Surjection of old gate ids to new gate ids. For using
     *                             the final (after transformation) operand in the gate (= rehung)
     * @return map, where the key is gate's operand and the value is how many times this operand
     *         appears in the gate
     */
    std::map<GateId, size_t>
    transformOperands_(CircuitT const& circuit, GateId gate_id, std::vector<GateId> const& old_to_new_gateId)
    {
        GateType gate_type = circuit.getGateType(gate_id);
        std::map<GateId, size_t> map_count_operands{};
        for (GateId operand : circuit.getGateOperands(gate_id))
        {
            // We should count only valid links (already changed).
            GateId check_operand = getLink_(operand, old_to_new_gateId);
            ++map_count_operands[check_operand];
        }

        GateIdContainer delete_key{};
        if (gate_type == GateType::XOR || gate_type == GateType::NXOR)
        {
            for (auto& [key, value] : map_count_operands)
            {
                //  For gates like XOR and NXOR we want to remove duplicates modulo two.
                value %= 2;
                if (value == 0)
                {
                    delete_key.push_back(key);
                }
            }
        }
        else if (
            gate_type == GateType::AND || gate_type == GateType::NAND || gate_type == GateType::OR ||
            gate_type == GateType::NOR)
        {
            // In the case of gates like AND, NAND, OR, NOR, we want to remove all duplicates
            for (auto& [_, value] : map_count_operands)
            {
                value = 1;
            }
        }
        else
        {
            //  the rest of the gates are left unchanged
        }

        for (auto key : delete_key)
        {
            map_count_operands.erase(key);
        }

        return map_count_operands;
    }

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
     * Checks if there are opposite operands in the gate
     * @param gate_info -- container of the new (transformer-modified) circuit
     * @param map_count_operands -- map, where the key is gate's operand and
     *                              the value is how many times this operand appears in the gate
     * @return boolean answer to checks (true or false)
     **/
    bool areThereOppositeOperands_(
        GateInfoContainer const& gate_info,
        std::map<GateId, size_t> const& map_count_operands)
    {
        // Due to the fact that new gates or new links can be
        // formed in the new circuit, we must use gate_info,
        // and not the circuit itself.
        return std::any_of(
            map_count_operands.begin(),
            map_count_operands.end(),
            [&gate_info, &map_count_operands](auto const& p)
            {
                return gate_info.at(p.first).getType() == GateType::NOT &&
                       map_count_operands.find(gate_info.at(p.first).getOperands().at(0)) != map_count_operands.end();
            });
    }

    /**
     * Removes opposite operands from gates of type XOR and NXOR
     * @param gate_info -- container of the new (transformer-modified) circuit
     * @param map_count_operands -- map, where the key is gate's operand and
     *                              the value is how many times this operand appears in the gate
     * @return operands for gate XOR/NXOR
     */
    GateIdContainer rebuildXORAndNXOR_(GateInfoContainer const& gate_info, std::map<GateId, size_t>& map_count_operands)
    {
        // Let's collect a complete list of opposite operands.
        size_t number_of_pair = 0;
        for (auto [operand, _] : map_count_operands)
        {
            if (gate_info.at(operand).getType() == GateType::NOT && map_count_operands[operand] > 0)
            {
                GateId const operand_of_not = gate_info.at(operand).getOperands().at(0);
                if (map_count_operands.find(operand_of_not) != map_count_operands.end() &&
                    map_count_operands[operand_of_not] > 0)
                {
                    --map_count_operands[operand];
                    --map_count_operands[operand_of_not];
                    ++number_of_pair;
                }
            }
        }

        // Form a list of operands without taking into account the found opposites
        GateIdContainer operands{};
        for (auto [operand, value] : map_count_operands)
        {
            for (size_t num_operands = 0; num_operands < value; ++num_operands)
            {
                operands.push_back(operand);
            }
        }

        // If the number of pairs of opposite operands was odd, then add one auxiliary gate of the CONST_TRUE type.
        if (number_of_pair % 2 == 1)
        {
            operands.push_back(id_const_true);
        }

        return operands;
    }
};

}  // namespace csat::simplification

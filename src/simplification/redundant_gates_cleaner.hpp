#pragma once

#include <cassert>
#include <memory>
#include <type_traits>
#include <utility>

#include "src/algo.hpp"
#include "src/common/csat_types.hpp"
#include "src/simplification/transformer_base.hpp"
#include "src/structures/circuit/gate_info.hpp"
#include "src/structures/circuit/icircuit.hpp"
#include "src/utility/encoder.hpp"
#include "src/utility/logger.hpp"

namespace csat::simplification
{

/**
 * Transformer, that cleans circuit from all
 * gates that are not reachable from outputs.
 *
 * @tparam CircuitT
 * @tparam preserveInputs -- if True, then Inputs won't be removed from a circuit.
 */
template<
    class CircuitT,
    bool preserveInputs = false,
    typename            = std::enable_if_t<std::is_base_of_v<ICircuit, CircuitT>>>
class RedundantGatesCleaner_ : public ITransformer<CircuitT>
{
    csat::Logger logger{"RedundantGatesCleaner"};

  public:
    /**
     * Applies RedundantGatesCleaner_ transformer to `circuit`
     * @param circuit -- circuit to transform.
     * @param encoder -- circuit encoder.
     * @return  circuit and encoder after transformation.
     */
    CircuitAndEncoder<CircuitT, std::string> transform(
        std::unique_ptr<CircuitT> circuit,
        std::unique_ptr<GateEncoder<std::string>> encoder)
    {
        logger.debug("=========================================================================================");
        logger.debug("START RedundantGatesCleaner.");

        GateEncoder<GateId> encoder_old_to_new{};
        // Use dfs to get markers of visited and unvisited gates
        auto mask_use_output = algo::performDepthFirstSearch(*circuit, circuit->getOutputGates());

        // first step: getting valid encoding
        for (GateId gateId = 0; gateId < circuit->getNumberOfGates(); ++gateId)
        {
            if (mask_use_output.at(gateId) != algo::DFSState::UNVISITED ||
                (preserveInputs && circuit->getGateType(gateId) == GateType::INPUT))
            {
                encoder_old_to_new.encodeGate(gateId);
                continue;
            }

            logger.debug("Gate number ", gateId, " is redundant and will be removed");
        }

        // Rebuid circuit
        GateInfoContainer gate_info(encoder_old_to_new.size());
        for (GateId gateId = 0; gateId < circuit->getNumberOfGates(); ++gateId)
        {
            if (encoder_old_to_new.keyExists(gateId))
            {
                GateIdContainer encoded_operands_{};
                for (GateId operand : circuit->getGateOperands(gateId))
                {
                    assert(
                        mask_use_output.at(gateId) != algo::DFSState::UNVISITED ||
                        (preserveInputs && circuit->getGateType(gateId) == GateType::INPUT));

                    encoded_operands_.push_back(encoder_old_to_new.encodeGate(operand));
                }
                gate_info.at(encoder_old_to_new.encodeGate(gateId)) = {
                    circuit->getGateType(gateId), std::move(encoded_operands_)};
            }
        }

        // All outputs must be visited since DFS starts from them.
        GateIdContainer new_output_gates{};
        new_output_gates.reserve(circuit->getOutputGates().size());
        for (GateId output_gate : circuit->getOutputGates())
        {
            new_output_gates.push_back(encoder_old_to_new.encodeGate(output_gate));
        }

        logger.debug("END RedundantGatesCleaner.");
        logger.debug("=========================================================================================");
        return {
            std::make_unique<CircuitT>(std::move(gate_info), std::move(new_output_gates)),
            utils::mergeGateEncoders(*encoder, encoder_old_to_new)};
    };
};

}  // namespace csat::simplification

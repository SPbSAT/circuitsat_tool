#pragma once

#include <memory>
#include <ranges>
#include <sstream>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "src/algo.hpp"
#include "src/simplification/transformer_base.hpp"
#include "src/utility/converters.hpp"

namespace csat::simplification
{

/**
 * Transformer, that cleans circuit from duplicate gates.
 * Duplicates are gates with the same operands and operator
 *
 * Note that this algorithm requires RedundantGatesCleaner to be applied right before.
 * Note that this algorithm will not reduce duplicate operands of gate, but will account
 * them "as one operand" during duplicates search. To reduce such gates one can use
 * separate `DuplicateOperandsCleaner` strategy.
 *
 * @tparam CircuitT
 */
template<class CircuitT, typename = std::enable_if_t<std::is_base_of_v<ICircuit, CircuitT> > >
class DuplicateGatesCleaner_ : public ITransformer<CircuitT>
{
    csat::Logger logger{"DuplicateGatesCleaner"};

  public:
    CircuitAndEncoder<CircuitT, std::string> transform(
        std::unique_ptr<CircuitT> circuit,
        std::unique_ptr<GateEncoder<std::string>> encoder)
    {
        logger.debug("=========================================================================================");
        logger.debug("START DuplicateGatesCleaner");

        GateEncoder<GateId> new_encoder{};

        // Topsort, from inputs to outputs.
        csat::GateIdContainer gateSorting(algo::TopSortAlgorithm<algo::DFSTopSort>::sorting(*circuit));
        std::reverse(gateSorting.begin(), gateSorting.end());

        BoolVector safe_mask(circuit->getNumberOfGates(), true);  // 0 -- if gate is a duplicate, 1 -- otherwise

        // `auxiliary_encoder` helps to deduplicate gates by mapping same auxiliary name to one index.
        GateEncoder<GateId> auxiliary_encoder{};
        // maps original gate ID to auxiliary ID, gotten from `auxiliary_encoder`.
        std::unordered_map<GateId, GateId> gate_id_to_auxiliary_id{};

        logger.debug("Building mask to delete gates and filling map -- gate_id_to_auxiliary_id");
        std::string auxiliary_name;
        for (GateId gateId : gateSorting)
        {
            logger.debug("Processing gate ", gateId);
            auxiliary_name.clear();
            auxiliary_name = formatGateAuxiliaryName_(
                gateId, circuit->getGateType(gateId), circuit->getGateOperands(gateId), gate_id_to_auxiliary_id);
            logger.debug("Auxiliary name for gate ", gateId, " is ", auxiliary_name);

            if (auxiliary_encoder.keyExists(auxiliary_name))
            {
                logger.debug("Gate number ", gateId, " is a Duplicate and will be removed.");
                safe_mask.at(gateId) = false;
            }
            else
            {
                logger.debug(
                    "Gate number ", gateId, " is either unique, or first of found duplicated, and will be saved.");
                new_encoder.encodeGate(encoder->decodeGate(gateId));
            }

            gate_id_to_auxiliary_id[gateId] = auxiliary_encoder.encodeGate(auxiliary_name);
        }

        logger.debug("Building new circuit");
        GateInfoContainer gate_info(auxiliary_encoder.size());
        for (GateId gateId = 0; gateId < circuit->getNumberOfGates(); ++gateId)
        {
            if (safe_mask.at(gateId))
            {
                logger.debug(
                    "New Gate ",
                    gate_id_to_auxiliary_id.at(gateId),
                    "; Type: ",
                    utils::gateTypeToString(circuit->getGateType(gateId)),
                    "; Operands: ",
                    formatOperandsString_(circuit->getGateOperands(gateId), gate_id_to_auxiliary_id).str());

                GateIdContainer masked_operands_{};
                for (GateId operand : circuit->getGateOperands(gateId))
                {
                    masked_operands_.push_back(gate_id_to_auxiliary_id.at(operand));
                }
                gate_info.at(gate_id_to_auxiliary_id.at(gateId)) = {circuit->getGateType(gateId), masked_operands_};
            }
        }

        GateIdContainer new_output_gates{};
        new_output_gates.reserve(circuit->getOutputGates().size());
        for (GateId output_gate : circuit->getOutputGates())
        {
            new_output_gates.push_back(gate_id_to_auxiliary_id.at(output_gate));
        }

        logger.debug("END DuplicateGatesCleaner");
        logger.debug("=========================================================================================");
        return {std::make_unique<CircuitT>(gate_info, new_output_gates), std::make_unique<GateEncoder>(new_encoder)};
    };

  private:
    /**
     * Formats new auxiliary name for the gate based on its attributes (type, operands).
     * Such name may be used to determine literal duplicates in the circuit.
     *
     * @param gateId -- original ID of gate.
     * @param gateType -- type of gate.
     * @param operands -- gate's operand IDs.
     * @param encoder -- mapping of circuit gates' original IDs to new names. Must already contain
     *        value for each operand of provided gate. It is needed to detect dependant duplicates
     *        in only one circuit iteration.
     * @return auxiliary name of gate.
     */
    std::string formatGateAuxiliaryName_(
        GateId gateId,
        GateType gateType,
        GateIdContainer const& operands,
        std::unordered_map<GateId, GateId> const& deduplicator)
    {
        std::stringstream auxiliary_name;
        auxiliary_name << std::to_string(static_cast<uint8_t>(gateType));

        // All Input gates are unique, but we can't differentiate them
        // by their operands (since there are none), so we simply add
        // their current (unique) index to auxiliary name and return.
        if (gateType == GateType::INPUT)
        {
            auxiliary_name << '_' + std::to_string(gateId);
            return auxiliary_name.str();
        }

        // Preparing set of operands, by accounting already found duplicates.
        GateIdContainer prepared_operands{};
        prepared_operands.reserve(operands.size());
        std::transform(
            operands.begin(),
            operands.end(),
            std::back_inserter(prepared_operands),
            [&deduplicator](GateId operand) { return deduplicator.at(operand); });

        // For gates defined by symmetrical function operands are sorted during
        // circuit construction (see GateInfo), but then some of their operands
        // may be decided to be duplicates, so we resort them so such gates with
        // same set of operands will have same auxiliary name and will be detected
        // as duplicates.
        if (utils::symmetricOperatorQ(gateType))
        {
            std::sort(prepared_operands.begin(), prepared_operands.end());
            // Since several operands of same gate may be decided to be duplicates
            // we can have here something like `AND(X, X, Y)`, which is equivalent
            // to the `AND(X, Y)`, hence to allow such duplicated to be found we
            // also clean such duplicates.
            if (utils::reducibleMultipleOperandsQ(gateType))
            {
                // Note: `unique` requires container to be sorted.
                auto last = std::unique(prepared_operands.begin(), prepared_operands.end());
                prepared_operands.erase(last, prepared_operands.end());
            }
        }

        // Adds names of gate's operands to its auxiliary name.
        for (GateId operand : prepared_operands)
        {
            auxiliary_name << '_' + std::to_string(operand);
        }

        return auxiliary_name.str();
    };

    std::stringstream formatOperandsString_(
        GateIdContainer const& operands,
        std::unordered_map<GateId, GateId> const& encoder)
    {
        std::stringstream ss;
        if (operands.empty())
        {
            return ss;
        }

        ss << encoder.at(operands[0]);
        for (auto ptr = operands.begin() + 1; ptr != operands.end(); ++ptr)
        {
            ss << ',' << encoder.at(*ptr);
        }
        return ss;
    }
};

}  // namespace csat::simplification

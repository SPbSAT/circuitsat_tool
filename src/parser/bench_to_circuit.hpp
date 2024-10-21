#pragma once

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <ostream>
#include <string_view>
#include <type_traits>

#include "src/common/csat_types.hpp"
#include "src/parser/ibench_parser.hpp"
#include "src/structures/circuit/gate_info.hpp"
#include "src/structures/circuit/icircuit.hpp"
#include "src/structures/circuit/icircuit_builder.hpp"
#include "src/utility/converters.hpp"
#include "src/utility/logger.hpp"

/**
 * Parser from `CircuitSAT.BENCH` file.
 */
namespace csat::parser
{

/**
 * CircuitSAT.BENCH parser.
 * @tparam CircuitT -- data structure that will
 * be returned by member-function `instantiate`.
 */
template<class CircuitT>
class BenchToCircuit : public IBenchParser, public ICircuitBuilder<CircuitT>
{
    static_assert(
        std::is_base_of<ICircuit, CircuitT>::value,
        "CircuitT template parameter must be a class, derived from ICircuit.");

  protected:
    /* Personal named logger. */
    Logger logger{"BenchToCircuit"};

    /* List of output gates. */
    GateIdContainer _output_gate_ids;
    /* Vector of gate info. */
    GateInfoContainer _gate_info_vector;

  public:
    BenchToCircuit()           = default;
    ~BenchToCircuit() override = default;

    /**
     * Clears internal state of a parser.
     */
    void clear() final
    {
        IBenchParser::encoder.clear();
        _output_gate_ids.clear();
        _gate_info_vector.clear();
    }

    /**
     * Instantiates d CircuitT.
     * @return Circuit instance, built according to current parser info.
     */
    std::unique_ptr<CircuitT> instantiate() final
    {
        return std::make_unique<CircuitT>(_gate_info_vector, _output_gate_ids);
    };

  protected:
    /**
     * Circuit input handler.
     * @param var_name -- name of processed variable.
     */
    void handleInput(GateId gateId) final
    {
        IBenchParser::logger.debug("\tEncoded name: \"", gateId, "\".");
        _addGate(gateId, GateType::INPUT, {});
    };

    /**
     * Circuit output handler.
     * @param var_name -- name of processed variable.
     */
    void handleOutput(GateId gateId) final
    {
        IBenchParser::logger.debug("\tEncoded name: \"", gateId, "\".");
        _output_gate_ids.push_back(gateId);
    };

    /**
     * Circuit gate handler.
     * @param op -- operation.
     * @param gateId -- gate.
     * @param var_operands -- operands of gate.
     */
    void handleGate(std::string_view op, GateId gateId, GateIdContainer const& var_operands) final
    {
        auto op_type = csat::utils::stringToGateType(std::string(op));
        _addGate(gateId, op_type, var_operands);
    };

    bool specialOperatorCallback_(GateId gateId, std::string_view op, std::string_view operands_str) final
    {
        // Specific gate, which is found in some benchmarks.
        // It has no operands, but contains a value (0 or 1).
        //
        // Note that `op` and `operands_str` spaces are already trimmed.
        if (op == "CONST")
        {
            if (operands_str == "0")
            {
                _addGate(gateId, GateType::CONST_FALSE, {});
            }
            else if (operands_str == "1")
            {
                _addGate(gateId, GateType::CONST_TRUE, {});
            }
            else
            {
                std::cerr << "Unsupported special operator CONST with operands\"" << operands_str << "\"" << std::endl;
                std::abort();
            }
            return true;
        }
        // Another specific gate is `vdd`, which is formed by ABC and is always equal to true
        else if (op == "vdd")
        {
            _addGate(gateId, GateType::CONST_TRUE, {});
            return true;
        }

        // `op` is not a special operator.
        return false;
    };

    /* Adds Gate info to parser internal state. */
    void _addGate(GateId gateId, GateType type, GateIdContainer const& operands) noexcept
    {
        assert(type != GateType::UNDEFINED);

        if (_gate_info_vector.size() <= gateId)
        {
            _gate_info_vector.resize(gateId + 1);
        }
        _gate_info_vector[gateId] = {type, operands};
    };
};

}  // namespace csat::parser

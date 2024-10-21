#pragma once

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>

#include "src/common/csat_types.hpp"
#include "src/parser/iparser.hpp"
#include "src/utility/encoder.hpp"
#include "src/utility/logger.hpp"
#include "src/utility/string_utils.hpp"

/**
 * Parser from `CircuitSAT.BENCH` file..
 */
namespace csat::parser
{

/**
 * Base class for CircuitSAT.BENCH parsers.
 */
class IBenchParser : public ICircuitParser
{
  public:
    ~IBenchParser() override = default;

    /**
     * Parser info from stream, that is .BENCH format file.
     * @param stream -- text stream, containing lines of some .BENCH file.
     */
    void parseStream(std::istream& stream) override
    {
        logger.info("Started parsing of BENCH stream.");
        std::string line;
        while (std::getline(stream, line))
        {
            parseBenchLine_(line);
        }
        _eof();
        logger.info("Ended parsing of BENCH stream.");
    }

    /* Encoder of inputs and gates. */
    csat::utils::GateEncoder<std::string> encoder;

    /**
     * @return Encoder, built according to parser info.
     */
    [[nodiscard]]
    csat::utils::GateEncoder<std::string> const& getEncoder() const
    {
        return encoder;
    }

  protected:
    /* Personal named logger. */
    Logger logger{"IBenchParser"};

    /**
     * Encode circuit variable.
     * @param var_name -- name of encoded variable.
     */
    virtual GateId encodeGate(std::string_view var_name)
    {
        return encoder.encodeGate(var_name);
    };

    /**
     * Circuit input handler.
     * @param var_name -- name of processed variable.
     */
    inline virtual void handleInput(GateId gateId) = 0;

    /**
     * Circuit output handler.
     * @param var_name -- name of processed variable.
     */
    inline virtual void handleOutput(GateId gateId) = 0;

    /**
     * Circuit gate handler.
     * @param op -- operation.
     * @param gateId -- gate.
     * @param var_operands -- operands of gate.
     */
    inline virtual void handleGate(std::string_view op, GateId gateId, GateIdContainer const& var_operands) = 0;

    /**
     * Handles specific operators which are found in some benchmarks.
     * e.g. CONST(0) and CONST(1) operators.
     * @param op -- operation.
     * @param gateId -- gate.
     * @param var_operands -- operands of gate.
     */
    inline virtual bool specialOperatorCallback_(GateId gateId, std::string_view op, std::string_view operands_str) = 0;

    /* Parses one line of bench file. */
    virtual void parseBenchLine_(std::string_view line)
    {
        logger.debug("Parsing Line: \"", line, "\".");
        csat::utils::string_utils::trimSpaces(line);
        if (line.empty() || line[0] == '#' || line == "\n")
        {
            logger.debug("\tReceived comment or empty line.");
            return;
        }
        else if (line.substr(0, 5) == "INPUT")
        {
            logger.debug("\tReceived input gate line.");
            std::string_view var_name = line.substr(6, line.find(')') - 6);
            csat::utils::string_utils::trimSpaces(var_name);

            logger.debug("\tEncoding input gate: \"", var_name, "\".");
            GateId const gateId = encodeGate(var_name);
            handleInput(gateId);
            return;
        }
        else if (line.substr(0, 6) == "OUTPUT")
        {
            logger.debug("\tReceived output gate line.");
            std::string_view var_name = line.substr(7, line.find(')') - 7);
            csat::utils::string_utils::trimSpaces(var_name);

            logger.debug("\tEncoding output gate: \"", var_name, "\".");
            GateId const gateId = encodeGate(var_name);
            handleOutput(gateId);
            return;
        }
        else
        {
            // Operator gate

            // Find special delimiters positions
            auto [eq_idx, l_bkt_idx, r_bkt_idx] = _getDelimitersPositions(line);

            std::string_view var_name = line.substr(0, eq_idx);
            csat::utils::string_utils::trimSpaces(var_name);

            std::string_view op = line.substr(eq_idx + 1, l_bkt_idx - eq_idx - 1);
            csat::utils::string_utils::trimSpaces(op);

            std::string_view operands_str = "";
            if (r_bkt_idx > l_bkt_idx)
            {
                operands_str = line.substr(l_bkt_idx + 1, r_bkt_idx - l_bkt_idx - 1);
                csat::utils::string_utils::trimSpaces(operands_str);
            }

            GateId const gateId = encodeGate(var_name);

            if (specialOperatorCallback_(gateId, op, operands_str))
            {
#ifdef ENABLE_DEBUG_LOGGING
                logger.debug(
                    "Line with specific operator is parsed. ",
                    "Operator: \"",
                    op,
                    "\";\n",
                    "\tEncoded Name: \"",
                    encoder.encodeGate(var_name),
                    "\";\n",
                    "\tOperands str: \"",
                    operands_str,
                    "\"");
#endif

                // If operator is specific it will be handled in the callback method, then no
                // additional processing is necessary
                return;
            }

            GateIdContainer var_operands;
            size_t comma_idx = 0;
            while ((comma_idx = operands_str.find(',')) != std::string::npos)
            {
                std::string_view operand = operands_str.substr(0, comma_idx);
                csat::utils::string_utils::trimSpaces(operand);
                var_operands.push_back(encodeGate(operand));
                operands_str = operands_str.substr(comma_idx + 1, operands_str.size() - comma_idx - 1);
            }
            csat::utils::string_utils::trimSpaces(operands_str);
            var_operands.push_back(encodeGate(operands_str));

#ifdef ENABLE_DEBUG_LOGGING
            logger.debug(
                "Line parsed. Operator: \"",
                op,
                "\";\n",
                "\tOperator: \"",
                op,
                "\";\n",
                "\tName: \"",
                var_name,
                "\";\n",
                "\tEncoded Name: \"",
                encoder.encodeGate(var_name),
                "\";\n",
                "\tOperands:\n");
            for (auto const& x : var_operands)
            {
                logger.debug("\t\"", x, "\".");
            }
#endif

            handleGate(op, gateId, var_operands);
        }
    };

    /* Post-parsing calculations of data structures. */
    virtual void _eof() {
        // empty
    };

    /* Returns delimiters positions ( '=', '(', ')' ) in an operator bench line. */
    std::tuple<size_t, size_t, size_t> _getDelimitersPositions(std::string_view line)
    {
        std::size_t line_size = line.size();
        // Find special delimiters positions
        std::size_t eq_idx    = std::string::npos;
        std::size_t l_bkt_idx = std::string::npos;
        std::size_t r_bkt_idx = std::string::npos;
        for (size_t idx = 0; idx < line_size; ++idx)
        {
            switch (line.at(idx))
            {
                case '=':
                    eq_idx = idx;
                    break;
                case '(':
                    l_bkt_idx = idx;
                    break;
                case ')':
                    r_bkt_idx = idx;
                    break;
                default:
                    break;
            }
        }

        // Check validity of delimiters position
        if (l_bkt_idx == std::string::npos)
        {
            std::string_view is_operator_vdd = line.substr(eq_idx + 1, line_size - eq_idx - 1);
            csat::utils::string_utils::trimSpaces(is_operator_vdd);
            if (is_operator_vdd == "vdd")
            {
                return {eq_idx, line_size, line_size};
            }
        }

        if (eq_idx == std::string::npos || l_bkt_idx == std::string::npos || r_bkt_idx == std::string::npos ||
            eq_idx >= l_bkt_idx || eq_idx >= r_bkt_idx || l_bkt_idx >= r_bkt_idx)
        {
            logger.error("\tCan't parse line: \"", line, "\"");
            throw std::runtime_error("Can't parse file");
        }

        return {eq_idx, l_bkt_idx, r_bkt_idx};
    }
};

}  // namespace csat::parser

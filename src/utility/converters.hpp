#pragma once

#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>

#include "src/common/csat_types.hpp"

namespace csat::utils
{

/**
 * @return Reference to a string->GateType map.
 */
[[maybe_unused]]
inline GateType stringToGateType(std::string const& type_name)
{
    static std::map<std::string, GateType, std::less<>> const _type_map{
        {"NOT",         GateType::NOT        },
        {"AND",         GateType::AND        },
        {"NAND",        GateType::NAND       },
        {"OR",          GateType::OR         },
        {"NOR",         GateType::NOR        },
        {"XOR",         GateType::XOR        },
        {"NXOR",        GateType::NXOR       },
        {"IFF",         GateType::IFF        },
        {"MUX",         GateType::MUX        },
#ifdef BUFF_IS_IFF
        {"BUFF",        GateType::IFF        },
#else
        {"BUFF", GateType::BUFF},
#endif
        {"CONST_FALSE", GateType::CONST_FALSE},
        {"CONST_TRUE",  GateType::CONST_TRUE },
        {"vdd",         GateType::CONST_TRUE }
    };

    return _type_map.at(type_name);
}

[[maybe_unused]]
inline std::string gateTypeToString(GateType state) noexcept
{
    static std::map<GateType, std::string, std::less<>> const _type_to_string{
        {GateType::INPUT,       "INPUT"      },
        {GateType::NOT,         "NOT"        },
        {GateType::AND,         "AND"        },
        {GateType::NAND,        "NAND"       },
        {GateType::OR,          "OR"         },
        {GateType::NOR,         "NOR"        },
        {GateType::XOR,         "XOR"        },
        {GateType::NXOR,        "NXOR"       },
        {GateType::IFF,         "IFF"        },
        {GateType::MUX,         "MUX"        },
        {GateType::BUFF,        "BUFF"       },
        {GateType::CONST_FALSE, "CONST_FALSE"},
        {GateType::CONST_TRUE,  "CONST_TRUE" }
    };

    return _type_to_string.at(state);
}

/**
 * @return New string object representing a basis name.
 */
[[maybe_unused]]
inline std::string basisToString(Basis basis) noexcept
{
    static std::unordered_map<Basis, std::string> const _type_map{
        {Basis::AIG,   "AIG"  },
        {Basis::BENCH, "BENCH"}
    };

    return _type_map.at(basis);
}

/**
 * @return Returns Basis element by given string.
 */
[[maybe_unused]]
inline Basis stringToBasis(std::string const& basis_name) noexcept
{
    static std::unordered_map<std::string, Basis> const _type_map{
        {"AIG",   Basis::AIG  },
        {"BENCH", Basis::BENCH}
    };

    return _type_map.at(basis_name);
}

[[maybe_unused]]
inline ReturnCode returnCodeNameToReturnCode(std::string_view state_name) noexcept
{
    return state_name == "SAT" ? ReturnCode::SAT : state_name == "UNSAT" ? ReturnCode::UNSAT : ReturnCode::UNDEFINED;
}

[[maybe_unused]]
inline std::string gateStateToString(GateState state) noexcept
{
    return state == GateState::TRUE ? "TRUE" : state == GateState::FALSE ? "FALSE" : "UNDEFINED";
}

[[maybe_unused]]
inline std::string gateStateToSATAnswer(GateState state) noexcept
{
    return state == GateState::TRUE ? "SAT" : state == GateState::FALSE ? "UNSAT" : "UNDEFINED";
}

[[maybe_unused]]
inline ReturnCode gateStateToReturnCode(GateState state) noexcept
{
    return state == GateState::TRUE  ? ReturnCode::SAT
         : state == GateState::FALSE ? ReturnCode::UNSAT
                                     : ReturnCode::UNDEFINED;
}

[[maybe_unused]]
inline std::string returnCodeToString(ReturnCode code) noexcept
{
    return code == ReturnCode::SAT ? "SAT" : code == ReturnCode::UNSAT ? "UNSAT" : "UNDEFINED";
}

/**
 * @return Minimum arity of a given `gate_type`.
 */
[[maybe_unused]]
inline MinArity gateTypeToMinArity(GateType gate_type) noexcept
{
    static std::unordered_map<GateType, MinArity> const _type_to_arity{
        {GateType::INPUT,       MinArity::NULLARY},
        {GateType::NOT,         MinArity::UNARY  },
        {GateType::AND,         MinArity::BINARY },
        {GateType::NAND,        MinArity::BINARY },
        {GateType::OR,          MinArity::BINARY },
        {GateType::NOR,         MinArity::BINARY },
        {GateType::XOR,         MinArity::BINARY },
        {GateType::NXOR,        MinArity::BINARY },
        {GateType::IFF,         MinArity::UNARY  },
        {GateType::MUX,         MinArity::TERNARY},
        {GateType::BUFF,        MinArity::UNARY  },
        {GateType::CONST_FALSE, MinArity::NULLARY},
        {GateType::CONST_TRUE,  MinArity::NULLARY}
    };

    return _type_to_arity.at(gate_type);
}

/**
 * @return True if the given `gate_type` can have arity greater than `gateTypeToMinArity(gate_type)`.
 */
[[maybe_unused]]
inline bool expandableArityQ(GateType gate_type) noexcept
{
    static std::unordered_map<GateType, bool> const _type_map{
        {GateType::INPUT,       0},
        {GateType::NOT,         0},
        {GateType::AND,         1},
        {GateType::NAND,        1},
        {GateType::OR,          1},
        {GateType::NOR,         1},
        {GateType::XOR,         1},
        {GateType::NXOR,        1},
        {GateType::IFF,         0},
        {GateType::MUX,         0},
        {GateType::BUFF,        0},
        {GateType::CONST_FALSE, 0},
        {GateType::CONST_TRUE,  0}
    };

    return _type_map.at(gate_type);
}

/**
 * @return True if operands can be swapped without changing the results.
 */
[[maybe_unused]]
inline bool symmetricOperatorQ(GateType gate_type) noexcept
{
    static std::unordered_map<GateType, bool> const _type_map{
        {GateType::INPUT,       true },
        {GateType::NOT,         true },
        {GateType::AND,         true },
        {GateType::NAND,        true },
        {GateType::OR,          true },
        {GateType::NOR,         true },
        {GateType::XOR,         true },
        {GateType::NXOR,        true },
        {GateType::IFF,         true },
        {GateType::MUX,         false},
        {GateType::BUFF,        true },
        {GateType::CONST_FALSE, true },
        {GateType::CONST_TRUE,  true }
    };

    return _type_map.at(gate_type);
}

}  // namespace csat::utils

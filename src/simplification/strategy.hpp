#pragma once

#include <type_traits>

#include "src/simplification/composition.hpp"
#include "src/simplification/constant_gate_reducer.hpp"
#include "src/simplification/duplicate_gates_cleaner.hpp"
#include "src/simplification/duplicate_operands_cleaner.hpp"
#include "src/simplification/reduce_not_composition.hpp"
#include "src/simplification/redundant_gates_cleaner.hpp"
#include "src/structures/circuit/dag.hpp"
#include "src/structures/circuit/icircuit.hpp"

namespace csat::simplification
{

/**
 * Transformer, that cleans circuit from all
 * gates that are not reachable from outputs.
 *
 * @tparam CircuitT
 */
template<class CircuitT, typename = std::enable_if_t<std::is_base_of_v<ICircuit, CircuitT> > >
using RedundantGatesCleaner =
    csat::simplification::Composition<CircuitT, csat::simplification::RedundantGatesCleaner_<csat::DAG> >;

/**
 * Transformer, that cleans circuit from duplicate gates.
 * Duplicates are gates with the same operands and operator
 *
 * @tparam CircuitT
 */
template<class CircuitT, typename = std::enable_if_t<std::is_base_of_v<ICircuit, CircuitT> > >
using DuplicateGatesCleaner = csat::simplification::Composition<
    CircuitT,
    csat::simplification::RedundantGatesCleaner_<CircuitT>,
    csat::simplification::DuplicateGatesCleaner_<CircuitT> >;

/**
 * Transformer, that cleans the circuit from unnecessary gates NOT.
 * For example: NOT(NOT(x)) => x
 *
 * @tparam CircuitT
 */
template<class CircuitT, typename = std::enable_if_t<std::is_base_of_v<ICircuit, CircuitT> > >
using ReduceNotComposition = csat::simplification::Composition<
    CircuitT,
    csat::simplification::ReduceNotComposition_<csat::DAG>,
    csat::simplification::RedundantGatesCleaner_<csat::DAG> >;

/**
 * Transformer, that cleans the circuit from constant gates ( like AND(x, NOT(x)) = false )
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
 * @tparam CircuitT
 */
template<class CircuitT, typename = std::enable_if_t<std::is_base_of_v<ICircuit, CircuitT> > >
using ConstantGateReducer = csat::simplification::Composition<
    CircuitT,
    csat::simplification::ConstantGateReducer_<csat::DAG>,
    csat::simplification::ReduceNotComposition_<csat::DAG>,
    csat::simplification::RedundantGatesCleaner_<csat::DAG>,
    csat::simplification::DuplicateGatesCleaner_<csat::DAG> >;

/**
 * Transformer, that cleans the circuit from gates with the same operands. For example:
 *
 *    Before            |          After
 *
 * INPUT(0)             |       INPUT(0)
 * INPUT(1)             |       INPUT(1)
 * INPUT(2)             |       INPUT(2)
 * 3 = NOT(0)           |       3 = NOT(0)
 * 4 = AND(3, 3)        |       5 = AND(0, 3)
 * 5 = AND(0, 4)        |       6 = OR(1, 2, 5)
 * 6 = OR(1, 2, 5)      |       OUTPUT(6)
 * OUTPUT(6)            |
 *
 * And after cleans the circuit from constant gates ( like AND(x, NOT(x)) = false )
 *
 *    Before            |          After
 *
 * INPUT(0)             |       INPUT(1)
 * INPUT(1)             |       INPUT(2)
 * INPUT(2)             |       6 = OR(1, 2)
 * 3 = NOT(0)           |       OUTPUT(6)
 * 5 = AND(0, 3)        |
 * 6 = OR(1, 2, 5)      |
 * OUTPUT(6)            |
 *
 * @tparam CircuitT
 */
template<class CircuitT, typename = std::enable_if_t<std::is_base_of_v<ICircuit, CircuitT> > >
using DuplicateOperandsCleaner = csat::simplification::Composition<
    CircuitT,
    csat::simplification::RedundantGatesCleaner_<csat::DAG>,
    csat::simplification::DuplicateOperandsCleaner_<csat::DAG>,
    csat::simplification::RedundantGatesCleaner_<csat::DAG, true>,  // true == save at least one input
    csat::simplification::ConstantGateReducer_<csat::DAG>,
    csat::simplification::ReduceNotComposition_<csat::DAG>,
    csat::simplification::RedundantGatesCleaner_<csat::DAG>,
    csat::simplification::DuplicateGatesCleaner_<csat::DAG> >;

}  // namespace csat::simplification

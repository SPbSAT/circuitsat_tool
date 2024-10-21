#include "src/utility/logger.hpp"
#include "src/parser/bench_to_circuit.hpp"
#include "src/structures/circuit/dag.hpp"

#include <string>
#include <unordered_map>
#include <vector>

#include "gtest/gtest.h"

namespace
{

csat::Logger logger("ParserTest");

using var_map = std::unordered_map<std::string, size_t>;
using gate_names = std::vector<std::string>;

void testBasicParse(
    std::string const& str,
    [[maybe_unused]] var_map map,
    gate_names const& gates)
{
    std::istringstream stream(str);
    csat::parser::BenchToCircuit<csat::DAG> parser;
    parser.parseStream(stream);
    auto circuit = parser.instantiate();
    for (auto const& g : gates)
    {
        logger.info("<PARSER TEST> Checking gate encoding: ", g, "==", parser.encoder.encodeGate(g));
        ASSERT_TRUE(map.at(g) == parser.encoder.encodeGate(g));
    }
}

TEST(BenchParser, One)
{
    std::string const test_case = "# Comment Line\n"
                                  "#\n"
                                  "\n"
                                  "INPUT(X)\n"
                                  "INPUT(Y)\n"
                                  "\n"
                                  "OUTPUT(Z)\n"
                                  "INPUT(F)\n";
    var_map const test_case_vm{{"X", 0}, {"Y", 1}, {"Z", 2}, {"F", 3}};
    gate_names gates{"X", "Y", "Z", "F"};
    testBasicParse(test_case, test_case_vm, gates);
}

TEST(BenchParser, Two)
{
    std::string const test_case = "# Comment Line\n"
                                  "#\n"
                                  "\n"
                                  "INPUT(X)\n"
                                  "INPUT(Y)\n"
                                  "\n"
                                  "OUTPUT(Z)\n"
                                  "Z = AND(X, Y)\n";
    var_map const test_case_vm{{"X", 0}, {"Y", 1}, {"Z", 2}};
    gate_names gates{"X", "Y", "Z"};
    testBasicParse(test_case, test_case_vm, gates);
}

TEST(BenchParser, LongNames)
{
    std::string const test_case = "# Comment Line\n"
                                  "#\n"
                                  "\n"
                                  "INPUT(INPUT)\n"
                                  "INPUT(ABCDEF)\n"
                                  "\n"
                                  "OUTPUT(OUTPUT)\n";
    var_map const test_case_vm{{"INPUT", 0}, {"ABCDEF", 1}, {"OUTPUT", 2}};
    gate_names gates{"INPUT", "ABCDEF", "OUTPUT"};
    testBasicParse(test_case, test_case_vm, gates);
}

TEST(BenchParser, Spaces)
{
    std::string const test_case = "# Comment Line\n"
                                  "#\n"
                                  "   \n"
                                  "INPUT(     XXX)\n"
                                  "INPUT(YY     )\n"
                                  "\n"
                                  "OUTPUT(   ZZZ   )\n";
    var_map const test_case_vm{{"XXX", 0}, {"YY", 1}, {"ZZZ", 2}};
    gate_names gates{"XXX", "YY", "ZZZ"};
    testBasicParse(test_case, test_case_vm, gates);
}

TEST(BenchParser, SpacesMux)
{
    std::string const test_case = "# Comment Line\n"
                                  "#\n"
                                  "   \n"
                                  " INPUT(     XXX)  \n"
                                  "INPUT(YY     )  \n"
                                  "  INPUT(  ZZZZ     )\n"
                                  "\n"
                                  "OUTPUT(   ABC   )\n"
                                  "   ABC  =   MUX(XXX  , YY,  ZZZZ  ) \n";
    var_map const test_case_vm{{"XXX", 0}, {"YY", 1}, {"ZZZZ", 2}, {"ABC", 3}};
    gate_names gates{"XXX", "YY", "ZZZZ", "ABC"};
    testBasicParse(test_case, test_case_vm, gates);
}

TEST(BenchParser, ExternalConst)
{
    std::string const test_case = "# Comment Line\n"
                                  "#\n"
                                  "   \n"
                                  " INPUT(     XXX)  \n"
                                  "INPUT(YY     )  \n"
                                  " ZZZZ  = CONST(0)\n"
                                  "\n"
                                  "OUTPUT(   ABC   )\n"
                                  "   ABC  =   MUX(XXX  , YY,  ZZZZ  ) \n";
    var_map const test_case_vm{{"XXX", 0}, {"YY", 1}, {"ZZZZ", 2}, {"ABC", 3}};
    gate_names gates{"XXX", "YY", "ZZZZ", "ABC"};
    testBasicParse(test_case, test_case_vm, gates);
}

TEST(BenchParser, ExternalConstGateType)
{
    std::string const test_case = "# Comment Line\n"
                                  "#\n"
                                  "   \n"
                                  " INPUT(     XXX)  \n"
                                  "INPUT(YY     )  \n"
                                  " ZZZZ  = CONST(0)\n"
                                  " FFFFF  = CONST(1)\n"
                                  "\n"
                                  "OUTPUT(   ABC   )\n"
                                  "   ABC  =   MUX(XXX  , YY,  ZZZZ  ) \n";
    
    std::istringstream stream(test_case);
    csat::parser::BenchToCircuit<csat::DAG> parser;
    parser.parseStream(stream);
    auto circuit = parser.instantiate();
    
    ASSERT_TRUE(0 == parser.encoder.encodeGate("XXX"));
    ASSERT_TRUE(1 == parser.encoder.encodeGate("YY"));
    ASSERT_TRUE(2 == parser.encoder.encodeGate("ZZZZ"));
    ASSERT_TRUE(3 == parser.encoder.encodeGate("FFFFF"));
    ASSERT_TRUE(4 == parser.encoder.encodeGate("ABC"));
    
    ASSERT_TRUE(circuit->getGateType(0) == csat::GateType::INPUT);
    ASSERT_TRUE(circuit->getGateType(1) == csat::GateType::INPUT);
    ASSERT_TRUE(circuit->getGateType(2) == csat::GateType::CONST_FALSE);
    ASSERT_TRUE(circuit->getGateType(3) == csat::GateType::CONST_TRUE);
    ASSERT_TRUE(circuit->getGateType(4) == csat::GateType::MUX);
}

TEST(BenchParser, VDD)
{
    std::string const test_case = "# Comment Line\n"
                                  "#\n"
                                  "   \n"
                                  " INPUT(     XXX)  \n"
                                  " ZZZZ  = vdd\n"
                                  " FFFFF  =      vdd    \n"
                                  "\n"
                                  "OUTPUT(   ABC   )\n"
                                  "   ABC  =   MUX(XXX  , FFFFF,  ZZZZ  ) \n";
    
    std::istringstream stream(test_case);
    csat::parser::BenchToCircuit<csat::DAG> parser;
    parser.parseStream(stream);
    auto circuit = parser.instantiate();
    
    ASSERT_TRUE(0 == parser.encoder.encodeGate("XXX"));
    ASSERT_TRUE(1 == parser.encoder.encodeGate("ZZZZ"));
    ASSERT_TRUE(2 == parser.encoder.encodeGate("FFFFF"));
    ASSERT_TRUE(3 == parser.encoder.encodeGate("ABC"));
    
    ASSERT_TRUE(circuit->getGateType(0) == csat::GateType::INPUT);
    ASSERT_TRUE(circuit->getGateType(1) == csat::GateType::CONST_TRUE);
    ASSERT_TRUE(circuit->getGateType(2) == csat::GateType::CONST_TRUE);
    ASSERT_TRUE(circuit->getGateType(3) == csat::GateType::MUX);
}

} // namespace

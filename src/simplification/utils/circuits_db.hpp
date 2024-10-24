#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "src/common/csat_types.hpp"
#include "src/utility/converters.hpp"

namespace csat::simplification
{

/**
 * Structure for storing a circuit database.
 */
struct CircuitDB
{
    std::map<std::vector<int32_t>, int32_t> subcircuit_pattern_to_index;
    std::vector<GateIdContainer> subcircuit_outputs;
    std::vector<std::vector<GateIdContainer>> gates_operands;
    std::vector<int32_t> OPER_number;
    std::vector<std::vector<GateType>> gates_operations;

    /**
     * Reads a database for simplification in a specific format.
     * @param db_path -- path to the database text file
     * @param basis -- the database basis in which it will be read
     */
    CircuitDB(std::filesystem::path const& db_path, Basis basis)
    {
        if (basis != Basis::BENCH and basis != Basis::AIG)
        {
            std::cerr << "Incorrect basis! Choose one of [AIG, BENCH]" << std::endl;
            std::abort();
        }
        if (!std::filesystem::exists(db_path))
        {
            std::cerr << "There is no small-circuit database at " << db_path.string() << std::endl;
            std::abort();
        }

        read_db(db_path);
    }

    /**
     * Reads a database in a BENCH format.
     * Each row of the database must encode a circuit. Where:
     *  -- the first number in the line is the number of inputs, which are numbered from 0 to this number - 1;
     *  -- the second number is the number of outputs;
     *  -- the subsequent "outputs" entries represent output codes, which are truth tables written in decimal form;
     *  -- the subsequent "outputs" entries represent indeces of outputs;
     *  -- the following is a description of the gates. A gate is an operator and the operand indices it uses.
     *
     * @param db_path -- path to the database text file
     */
    void read_db(std::filesystem::path const& db_path)
    {
        // Creates ifstream object to read from the file whose path is passed as an argument.
        std::ifstream database(db_path);

        int32_t subcircuit_index = 0;
        size_t inputs_number     = 0;

        // A loop is started that continues as long as data can be read from the file.
        // The number of inputs is read first.
        while (database >> inputs_number)
        {
            // The number of outputs for the current circuit is read.
            size_t outputs_number = 0;
            database >> outputs_number;

            // Read the output codes
            std::vector<int32_t> outputs_patterns(outputs_number);
            for (size_t i = 0; i < outputs_number; ++i)
            {
                database >> outputs_patterns[i];
            }
            subcircuit_pattern_to_index[outputs_patterns] = subcircuit_index;

            // Read the output indices and determine their maximum index for further gate parsing
            std::vector<GateId> cur_outputs(outputs_number);
            GateId max_index = 0;
            for (size_t i = 0; i < outputs_number; ++i)
            {
                database >> cur_outputs[i];
                max_index = std::max(max_index, cur_outputs[i]);
            }
            subcircuit_outputs.push_back(cur_outputs);

            // Parse gates
            gates_operands.emplace_back();
            gates_operations.emplace_back();
            OPER_number.push_back(0);

            for (size_t i = inputs_number; i <= max_index; ++i)
            {
                std::string operation;
                GateIdContainer operands;
                GateId operand_1 = 0;
                GateId operand_2 = 0;

                // The database uses only basic gate types (i.e. it doesn't use IFF, BUFF, MUX, CONST_FALSE
                // and CONST_TRUE), and also it works only with binary gates except for NOT, which is unary.
                database >> operation;
                gates_operations[subcircuit_index].push_back(csat::utils::stringToGateType(operation));

                database >> operand_1;
                operands.push_back(operand_1);
                max_index = std::max(max_index, operand_1);

                if (operation != "NOT")
                {
                    database >> operand_2;
                    operands.push_back(operand_2);
                    max_index = std::max(max_index, operand_2);
                    ++OPER_number[subcircuit_index];
                }

                gates_operands[subcircuit_index].push_back(operands);
            }
            ++subcircuit_index;
        }
    }
};

/**
 * Carries globally accessible pointers to the database of optimal circuits.
 */
struct DBSingleton
{
  public:
    std::shared_ptr<CircuitDB> bench_db = nullptr;
    std::shared_ptr<CircuitDB> aig_db   = nullptr;

    DBSingleton(DBSingleton const&)            = delete;
    DBSingleton& operator=(DBSingleton const&) = delete;

    static DBSingleton& getInstance()
    {
        static DBSingleton s;
        return s;
    }

    static std::shared_ptr<CircuitDB> getAigDB()
    {
        if (DBSingleton::getInstance().aig_db == nullptr)
        {
            std::cerr << "Aig database is not available, aborting." << std::endl;
            std::abort();
        }
        return DBSingleton::getInstance().aig_db;
    }

    static std::shared_ptr<CircuitDB> getBenchDB()
    {
        if (DBSingleton::getInstance().bench_db == nullptr)
        {
            std::cerr << "Bench database is not available, aborting." << std::endl;
            std::abort();
        }
        return DBSingleton::getInstance().bench_db;
    }

  private:
    DBSingleton()  = default;
    ~DBSingleton() = default;
};

}  // namespace csat::simplification

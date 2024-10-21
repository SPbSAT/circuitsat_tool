#pragma once

#include <memory>
#include <random>
#include <string>
#include <type_traits>
#include <utility>

#include "src/common/csat_types.hpp"
#include "src/structures/circuit/icircuit.hpp"
#include "src/utility/encoder.hpp"
#include "src/utility/random.hpp"

namespace csat::simplification
{

using csat::utils::GateEncoder;

template<class CircuitT, class KeyT, typename = std::enable_if_t<std::is_base_of_v<ICircuit, CircuitT>>>
using CircuitAndEncoder = std::pair<std::unique_ptr<CircuitT>, std::unique_ptr<GateEncoder<KeyT>>>;

/**
 * Base interface for all circuit transformers.
 *
 * @tparam CircuitT -- type of a circuit structure.
 */
template<class CircuitT, typename = std::enable_if_t<std::is_base_of_v<ICircuit, CircuitT>>>
class ITransformer
{
  public:
    CircuitAndEncoder<CircuitT, std::string> apply(CircuitT const& circuit, GateEncoder<std::string> const& encoder)
    {
        return transform(std::make_unique<CircuitT>(circuit), std::make_unique<GateEncoder<std::string>>(encoder));
    }

    virtual CircuitAndEncoder<CircuitT, std::string> transform(
        std::unique_ptr<CircuitT>,
        std::unique_ptr<GateEncoder<std::string>>) = 0;
};

static std::string getUniqueId_()
{
    // Currently not the best way of random number generation
    // is presented, but it should be enough since number of
    // transformers applied to a circuit is relatively low.
    static auto engine(utils::getNewMersenneTwisterEngine());
    static std::uniform_int_distribution<> dist(100'000'000, 999'999'999);

    return std::to_string(dist(engine));
}

inline std::string getNewGateName_(std::string const& prefix, GateId id)
{
    return prefix + std::to_string(id);
}

inline std::string getNewGateName_(std::string const& prefix, std::string&& id)
{
    return prefix + id;
}

}  // namespace csat::simplification

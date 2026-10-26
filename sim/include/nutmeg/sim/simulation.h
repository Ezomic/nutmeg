#pragma once

#include "nutmeg/core/input.h"
#include "nutmeg/core/rng.h"
#include "nutmeg/gameplay/world.h"

#include <cstddef>
#include <cstdint>

namespace nutmeg {

// The one place the world advances. Live play and replay both call tick().
class Simulation {
public:
    static constexpr double kStepSeconds = 1.0 / 60.0;

    explicit Simulation(std::uint64_t seed)
        : rng_(Rng(seed).derive("sim"))
    {
    }

    void tick(const InputFrame& input);

    // The human input drives this player until controls assign one.
    static constexpr std::size_t kControlledPlayer = 10;

    const gameplay::World& world() const { return world_; }
    std::uint64_t stateHash() const;

private:
    gameplay::World world_;
    Rng rng_;
};

}

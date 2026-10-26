#pragma once

#include "nutmeg/core/input.h"
#include "nutmeg/core/rng.h"

#include <cstdint>

namespace nutmeg {

// A placeholder world: enough state to hash, so the loop and the replay path
// can be tested before the physics module defines the real one.
struct SimState {
    std::uint64_t tick = 0;
    float x = 0.0f;
    float y = 0.0f;
    std::uint32_t buttonsSeen = 0;
};

// The one place the world advances. Live play and replay both call tick().
class Simulation {
public:
    static constexpr double kStepSeconds = 1.0 / 60.0;

    explicit Simulation(std::uint64_t seed)
        : rng_(Rng(seed).derive("sim"))
    {
    }

    void tick(const InputFrame& input);

    const SimState& state() const { return state_; }
    std::uint64_t stateHash() const;

private:
    SimState state_;
    Rng rng_;
};

}

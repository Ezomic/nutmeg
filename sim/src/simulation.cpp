#include "nutmeg/sim/simulation.h"

#include "nutmeg/core/hash.h"

namespace nutmeg {

void Simulation::tick(const InputFrame& input)
{
    gameplay::PlayerInputs inputs{};
    inputs[kControlledPlayer] = input;
    world_.step(inputs);
}

std::uint64_t Simulation::stateHash() const
{
    return hashCombine(world_.hash(), mix64(rng_.state()));
}

}

#include "nutmeg/sim/simulation.h"

#include "nutmeg/core/hash.h"

namespace nutmeg {

void Simulation::tick(const InputFrame& input)
{
    gameplay::PlayerInputs inputs{};
    inputs[kControlledPlayer] = input;
    const gameplay::PlayerActions actions = lifecycle_.before(world_);
    if (lifecycle_.playing()) {
        world_.step(inputs, actions);
    }
    lifecycle_.after(world_);
}

std::uint64_t Simulation::stateHash() const
{
    return hashCombine(hashCombine(world_.hash(), lifecycle_.hash()), mix64(rng_.state()));
}

}

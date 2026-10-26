#include "nutmeg/sim/simulation.h"

#include "nutmeg/core/hash.h"

#include <bit>

namespace nutmeg {

void Simulation::tick(const InputFrame& input)
{
    const float jitter = (rng_.nextFloat() - 0.5f) * 0.001f;
    state_.x += input.moveX * static_cast<float>(kStepSeconds) + jitter;
    state_.y += input.moveY * static_cast<float>(kStepSeconds);
    state_.buttonsSeen |= input.buttons;
    ++state_.tick;
}

std::uint64_t Simulation::stateHash() const
{
    std::uint64_t h = mix64(state_.tick);
    h = hashCombine(h, mix64(std::bit_cast<std::uint32_t>(state_.x)));
    h = hashCombine(h, mix64(std::bit_cast<std::uint32_t>(state_.y)));
    h = hashCombine(h, mix64(state_.buttonsSeen));
    h = hashCombine(h, mix64(rng_.state()));
    return h;
}

}

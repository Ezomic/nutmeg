#include "nutmeg/gameplay/player.h"

#include "nutmeg/core/hash.h"

#include <bit>

namespace nutmeg::gameplay {

namespace {

std::uint64_t bits(float v) { return mix64(std::bit_cast<std::uint32_t>(v)); }

}

std::uint64_t Player::hash() const
{
    std::uint64_t h = mix64(id);
    h = hashCombine(h, bits(position.x));
    h = hashCombine(h, bits(position.y));
    h = hashCombine(h, bits(velocity.x));
    h = hashCombine(h, bits(velocity.y));
    h = hashCombine(h, bits(facing));
    h = hashCombine(h, bits(stamina));
    return h;
}

}

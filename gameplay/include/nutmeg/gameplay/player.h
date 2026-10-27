#pragma once

#include "nutmeg/physics/pitch.h"
#include "nutmeg/physics/vec.h"
#include "nutmeg/schema/player.h"

#include <cstdint>

namespace nutmeg::gameplay {

struct Player {
    std::uint32_t id = 0;
    physics::Side side = physics::Side::Home;
    physics::Vec2 position;
    physics::Vec2 velocity;
    float facing = 0.0f; // radians, 0 along +x, counter-clockwise, in [-pi, pi]
    float stamina = 1.0f; // placeholder: 0..1, not drained yet
    int cooldown = 0; // ticks before this player may touch or kick the ball again
    bool sentOff = false;
    schema::PlayerProfile profile;

    std::uint64_t hash() const;
};

}

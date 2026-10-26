#pragma once

#include "nutmeg/core/input.h"
#include "nutmeg/gameplay/player.h"
#include "nutmeg/physics/ball.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace nutmeg::gameplay {

inline constexpr std::size_t kPlayersPerSide = 11;
inline constexpr std::size_t kPlayerCount = 2 * kPlayersPerSide;

using PlayerInputs = std::array<InputFrame, kPlayerCount>;

// Players 0..10 are Home, 11..21 Away. Within a side index 0 is the keeper.
class World {
public:
    // Default-rated players in a 4-4-2 on their own halves.
    World();
    explicit World(const std::array<schema::PlayerProfile, kPlayerCount>& profiles);

    // Input is relative to the attacking direction: moveY is towards the
    // opponent's goal, moveX towards the right-hand touchline as the player
    // faces it. Sprint is a button.
    void step(const PlayerInputs& inputs);

    const std::array<Player, kPlayerCount>& players() const { return players_; }
    std::array<Player, kPlayerCount>& players() { return players_; }
    const physics::Ball& ball() const { return ball_; }
    std::uint64_t tick() const { return tick_; }
    std::uint64_t hash() const;

private:
    std::array<Player, kPlayerCount> players_;
    physics::Ball ball_;
    std::uint64_t tick_ = 0;
};

}

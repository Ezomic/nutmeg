#pragma once

#include "nutmeg/controls/vec.h"

#include <cstddef>
#include <optional>
#include <span>

namespace nutmeg::controls {

struct SwitcherConfig {
    // A challenger must be this much closer (metres) than the current player to take over.
    float hysteresis = 2.0f;
    // After a manual switch, auto-switching is paused for this many ticks.
    int manualHoldTicks = 60;
    // Aim at where the ball will be this far ahead instead of where it is.
    float predictSeconds = 0.0f;
};

struct SwitchInput {
    std::span<const Vec2> players; // the human's team, in a stable order from tick to tick
    Vec2 ball;
    Vec2 ballVelocity;
    bool manualSwitch = false;
    // Set to pin control to one player, e.g. the keeper on a goal kick.
    std::optional<std::size_t> lockedTo;
};

class ControllerSwitcher {
public:
    explicit ControllerSwitcher(SwitcherConfig config = {}) : config_(config) {}

    // Call once per tick. Returns the index into input.players of the controlled player.
    std::size_t update(const SwitchInput& input);

    std::size_t current() const { return current_; }

    bool operator==(const ControllerSwitcher&) const = default;

private:
    SwitcherConfig config_;
    std::size_t current_ = 0;
    bool initialised_ = false;
    int holdTicks_ = 0;
};

}

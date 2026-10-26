#include "nutmeg/controls/controller_switcher.h"

#include <cmath>

namespace nutmeg::controls {

namespace {

float distanceBetween(Vec2 a, Vec2 b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

// Lower index wins ties so the choice never depends on iteration luck.
std::size_t nearestIndex(std::span<const Vec2> players, Vec2 target)
{
    std::size_t best = 0;
    float bestDistance = distanceBetween(players[0], target);
    for (std::size_t i = 1; i < players.size(); ++i) {
        const float d = distanceBetween(players[i], target);
        if (d < bestDistance) {
            bestDistance = d;
            best = i;
        }
    }
    return best;
}

std::size_t rankOf(std::span<const Vec2> players, Vec2 target, std::size_t index)
{
    const float own = distanceBetween(players[index], target);
    std::size_t rank = 0;
    for (std::size_t i = 0; i < players.size(); ++i) {
        const float d = distanceBetween(players[i], target);
        if (d < own || (d == own && i < index)) {
            ++rank;
        }
    }
    return rank;
}

std::size_t playerAtRank(std::span<const Vec2> players, Vec2 target, std::size_t rank)
{
    for (std::size_t i = 0; i < players.size(); ++i) {
        if (rankOf(players, target, i) == rank) {
            return i;
        }
    }
    return 0;
}

}

std::size_t ControllerSwitcher::update(const SwitchInput& input)
{
    if (input.players.empty()) {
        return current_;
    }
    if (holdTicks_ > 0) {
        --holdTicks_;
    }

    const Vec2 target{
        input.ball.x + input.ballVelocity.x * config_.predictSeconds,
        input.ball.y + input.ballVelocity.y * config_.predictSeconds,
    };

    if (input.lockedTo && *input.lockedTo < input.players.size()) {
        current_ = *input.lockedTo;
        initialised_ = true;
        return current_;
    }
    if (!initialised_ || current_ >= input.players.size()) {
        current_ = nearestIndex(input.players, target);
        initialised_ = true;
        return current_;
    }

    if (input.manualSwitch) {
        const std::size_t next = (rankOf(input.players, target, current_) + 1) % input.players.size();
        current_ = playerAtRank(input.players, target, next);
        holdTicks_ = config_.manualHoldTicks;
        return current_;
    }

    if (holdTicks_ == 0) {
        const std::size_t nearest = nearestIndex(input.players, target);
        const float challenger = distanceBetween(input.players[nearest], target);
        if (nearest != current_ && challenger + config_.hysteresis < distanceBetween(input.players[current_], target)) {
            current_ = nearest;
        }
    }
    return current_;
}

}

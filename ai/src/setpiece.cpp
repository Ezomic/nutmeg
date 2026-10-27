#include "nutmeg/ai/setpiece.h"

#include "nutmeg/ai/bot.h"

#include <algorithm>
#include <array>

namespace nutmeg::ai {

namespace {

namespace tune = setpiece_tuning;
using gameplay::ActionKind;
using gameplay::ActionRequest;
using gameplay::kNoPlayer;
using gameplay::kPlayersPerSide;
using gameplay::Player;
using gameplay::Restart;
using gameplay::RestartKind;
using physics::Side;
using physics::Vec2;

float sideSign(Side s) { return s == Side::Home ? 1.0f : -1.0f; }
std::size_t firstIndex(Side s) { return s == Side::Home ? 0 : kPlayersPerSide; }

float openness(const gameplay::World& world, Side side, Vec2 at)
{
    const std::size_t base = firstIndex(physics::opposite(side));
    float best = 1e9f;
    for (std::size_t i = 0; i < kPlayersPerSide; ++i) {
        best = std::min(best, physics::distance(world.players()[base + i].position, at));
    }
    return std::min(best, 15.0f);
}

bool outfielder(const Player& p, const Player& taker)
{
    return p.side == taker.side && p.id != taker.id && !p.sentOff && p.id != firstIndex(p.side);
}

// The teammate in [minDistance, maxDistance] who is most open and furthest forward.
std::uint32_t bestMate(const gameplay::World& world, const Player& taker, float minDistance, float maxDistance, bool inBox)
{
    const Side attacking = taker.side;
    std::uint32_t best = kNoPlayer;
    float bestScore = -1e9f;
    for (const Player& mate : world.players()) {
        if (!outfielder(mate, taker)) {
            continue;
        }
        const float dist = physics::distance(mate.position, taker.position);
        if (dist < minDistance || dist > maxDistance) {
            continue;
        }
        if (inBox && !physics::pitch::inPenaltyArea(mate.position, physics::opposite(attacking))) {
            continue;
        }
        const float progress = (mate.position.x - taker.position.x) * sideSign(attacking);
        const float score = tune::kOpenWeight * openness(world, attacking, mate.position) + tune::kProgressWeight * progress - tune::kDistanceWeight * dist;
        if (score > bestScore) {
            bestScore = score;
            best = mate.id;
        }
    }
    return best;
}

ActionRequest groundPass(const Player& taker, const Player& mate)
{
    ActionRequest request;
    request.kind = ActionKind::Pass;
    request.target = mate.id;
    request.aim = mate.position;
    request.power = gameplay::passPowerForDistance(physics::distance(taker.position, mate.position));
    return request;
}

ActionRequest throwTo(const Player& taker, const Player& mate)
{
    constexpr float flightSeconds = 1.0f;
    ActionRequest request;
    request.kind = ActionKind::Pass;
    request.target = mate.id;
    request.aim = mate.position;
    const float speed = physics::distance(taker.position, mate.position) / flightSeconds;
    request.power = std::clamp((speed - gameplay::possession::kPassSpeedMin) / (gameplay::possession::kPassSpeedMax - gameplay::possession::kPassSpeedMin), 0.0f, 1.0f);
    return request;
}

ActionRequest cross(const Player* mate, Vec2 fallback)
{
    ActionRequest request;
    request.kind = ActionKind::Cross;
    request.target = mate != nullptr ? mate->id : kNoPlayer;
    request.aim = mate != nullptr ? mate->position : fallback;
    return request;
}

ActionRequest shot(const Player& taker, float power, bool lofted)
{
    const Side defending = physics::opposite(taker.side);
    const float corner = (taker.id & 1u) != 0 ? 1.0f : -1.0f;
    ActionRequest request;
    request.kind = ActionKind::Shoot;
    request.power = power;
    request.lofted = lofted;
    request.aim = {physics::pitch::goalLineX(defending), corner * physics::pitch::kHalfGoalWidth * tune::kCornerAimFraction};
    return request;
}

ActionRequest crossIntoBox(const gameplay::World& world, const Player& taker)
{
    const std::uint32_t mate = bestMate(world, taker, tune::kMateMinDistance, 60.0f, true);
    const Vec2 penaltySpot = physics::pitch::penaltySpot(physics::opposite(taker.side));
    return cross(mate != kNoPlayer ? &world.players()[mate] : nullptr, penaltySpot);
}

ActionRequest pass(const gameplay::World& world, const Player& taker, float maxDistance, bool thrown)
{
    const std::uint32_t mate = bestMate(world, taker, tune::kMateMinDistance, maxDistance, false);
    if (mate == kNoPlayer) {
        ActionRequest request;
        request.kind = ActionKind::Pass;
        request.aim = {0.0f, 0.0f};
        request.power = 0.5f;
        return request;
    }
    const Player& target = world.players()[mate];
    return thrown ? throwTo(taker, target) : groundPass(taker, target);
}

int delayFor(RestartKind kind)
{
    const bool chance = kind == RestartKind::Corner || kind == RestartKind::DirectFreeKick || kind == RestartKind::IndirectFreeKick;
    return chance ? tune::kSetupDelayTicks : tune::kTakeDelayTicks;
}

ActionRequest freeKick(const gameplay::World& world, const Restart& restart, const Player& taker)
{
    const Vec2 goal = physics::pitch::goalCentre(physics::opposite(restart.side));
    const float toGoal = physics::distance(restart.spot, goal);
    if (restart.kind == RestartKind::DirectFreeKick && toGoal <= tune::kShootRange) {
        return shot(taker, tune::kShotPower, toGoal <= tune::kChipRange);
    }
    if (restart.spot.x * sideSign(restart.side) >= tune::kCrossMinX) {
        return crossIntoBox(world, taker);
    }
    return pass(world, taker, 30.0f, false);
}

}

ActionRequest planRestart(const gameplay::World& world, const Restart& restart)
{
    if (restart.taker >= gameplay::kPlayerCount || world.tick() < world.restartSince() + delayFor(restart.kind)) {
        return {};
    }
    const Player& taker = world.players()[restart.taker];

    switch (restart.kind) {
    case RestartKind::Kickoff:
        return pass(world, taker, 25.0f, false);
    case RestartKind::ThrowIn:
        return pass(world, taker, tune::kThrowMaxDistance, true);
    case RestartKind::GoalKick: {
        const std::uint32_t mate = bestMate(world, taker, tune::kGoalKickMinDistance, 60.0f, false);
        return cross(mate != kNoPlayer ? &world.players()[mate] : nullptr, {0.0f, 0.0f});
    }
    case RestartKind::Corner:
        return crossIntoBox(world, taker);
    case RestartKind::Penalty:
        return shot(taker, tune::kShotPower, false);
    case RestartKind::DirectFreeKick:
    case RestartKind::IndirectFreeKick:
        return freeKick(world, restart, taker);
    }
    return {};
}

void positionForRestart(const gameplay::World& world, const Restart& restart, gameplay::PlayerInputs& inputs)
{
    const bool chance = restart.kind == RestartKind::Corner || restart.kind == RestartKind::DirectFreeKick || restart.kind == RestartKind::IndirectFreeKick;
    const Side defending = physics::opposite(restart.side);
    const Vec2 goal = physics::pitch::goalCentre(defending);
    if (!chance || physics::distance(restart.spot, goal) > tune::kBoxRange) {
        return;
    }

    const float inward = -sideSign(restart.side);
    const std::array<Vec2, tune::kBoxRunners> slots = {{
        {goal.x + inward * 11.0f, 0.0f},
        {goal.x + inward * 6.0f, 4.0f},
        {goal.x + inward * 6.0f, -4.0f},
        {goal.x + inward * 16.0f, restart.spot.y > 0.0f ? -8.0f : 8.0f},
    }};

    std::array<std::uint32_t, kPlayersPerSide> order{};
    std::size_t n = 0;
    for (const Player& p : world.players()) {
        if (p.side == restart.side && !p.sentOff && p.id != restart.taker && p.id != firstIndex(p.side)) {
            order[n++] = p.id;
        }
    }
    const float attack = sideSign(restart.side);
    std::stable_sort(order.begin(), order.begin() + static_cast<std::ptrdiff_t>(n), [&](std::uint32_t a, std::uint32_t b) {
        return world.players()[a].position.x * attack > world.players()[b].position.x * attack;
    });
    for (std::size_t i = 0; i < std::min<std::size_t>(n, slots.size()); ++i) {
        inputs[order[i]] = moveToward(world.players()[order[i]], {slots[i], tuning::kSprintUrgency});
    }
}

}

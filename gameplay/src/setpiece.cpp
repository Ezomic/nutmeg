#include "nutmeg/gameplay/setpiece.h"

#include "nutmeg/gameplay/world.h"

#include "nutmeg/core/dmath.h"
#include "nutmeg/physics/pitch.h"

#include <algorithm>
#include <cmath>

namespace nutmeg::gameplay {

namespace {

using physics::Side;
using physics::Vec2;
namespace tune = setpiece;

float attackSign(Side side) { return side == Side::Home ? 1.0f : -1.0f; }

std::size_t firstIndex(Side side) { return side == Side::Home ? 0 : kPlayersPerSide; }

bool bestRated(RestartKind kind)
{
    return kind == RestartKind::Penalty || kind == RestartKind::DirectFreeKick || kind == RestartKind::Corner;
}

float rating(const Player& p, RestartKind kind)
{
    const auto& a = p.profile.attributes;
    switch (kind) {
    case RestartKind::Penalty:
        return static_cast<float>(a.finishing + a.composure);
    case RestartKind::DirectFreeKick:
        return static_cast<float>(a.finishing + a.passing);
    default:
        return static_cast<float>(a.crossing + a.passing);
    }
}

std::uint32_t pickTaker(const Player* players, std::size_t count, const Restart& restart)
{
    const std::size_t keeper = firstIndex(restart.side);
    if (restart.kind == RestartKind::GoalKick && !players[keeper].sentOff) {
        return players[keeper].id;
    }
    std::uint32_t best = kNoPlayer;
    float bestScore = -1e9f;
    for (std::size_t i = 0; i < count; ++i) {
        const Player& p = players[i];
        if (p.side != restart.side || p.sentOff || i == keeper) {
            continue;
        }
        const float score = bestRated(restart.kind) ? rating(p, restart.kind) : -physics::distance(p.position, restart.spot);
        if (score > bestScore) {
            bestScore = score;
            best = p.id;
        }
    }
    return best;
}

Vec2 clampToPitch(Vec2 p)
{
    return {std::clamp(p.x, -physics::pitch::kHalfLength, physics::pitch::kHalfLength), std::clamp(p.y, -physics::pitch::kHalfWidth, physics::pitch::kHalfWidth)};
}

Vec2 unitOr(Vec2 v, Vec2 fallback)
{
    const float len = physics::length(v);
    return len > 1e-4f ? v * (1.0f / len) : fallback;
}

void pushOut(Player& p, Vec2 spot, float radius)
{
    if (physics::distance(p.position, spot) >= radius) {
        return;
    }
    const Vec2 inward = unitOr(Vec2{0.0f, 0.0f} - spot, Vec2{1.0f, 0.0f});
    Vec2 moved = clampToPitch(spot + unitOr(p.position - spot, inward) * radius);
    if (physics::distance(moved, spot) < radius - 0.01f) {
        moved = spot + inward * radius;
    }
    p.position = moved;
    p.velocity = {};
}

void clearKickoff(Player& p, const Restart& restart)
{
    const float own = p.side == Side::Home ? -1.0f : 1.0f;
    if (p.position.x * own < 0.0f) {
        p.position.x = 0.0f;
        p.velocity = {};
    }
    if (p.side == restart.side) {
        return;
    }
    const float y = std::min(std::abs(p.position.y), tune::kClearRadius);
    const float minX = std::sqrt(tune::kClearRadius * tune::kClearRadius - y * y);
    if (p.position.x * own < minX) {
        p.position.x = own * minX;
        p.velocity = {};
    }
}

void clearPenalty(Player& p, const Restart& restart)
{
    const Side defending = physics::opposite(restart.side);
    if (physics::pitch::inPenaltyArea(p.position, defending) || physics::distance(p.position, restart.spot) < tune::kClearRadius) {
        const float inward = defending == Side::Home ? 1.0f : -1.0f;
        p.position.x = physics::pitch::goalLineX(defending) + inward * tune::kPenaltyClearDepth;
        p.velocity = {};
    }
}

}

namespace setpiece {

std::optional<Restart> restartAfter(const MatchEvent& event, Vec2 ball)
{
    namespace pitch = physics::pitch;
    Restart restart;
    if (event.kind == EventKind::Goal) {
        restart.kind = RestartKind::Kickoff;
        restart.side = physics::opposite(event.side);
        restart.spot = {0.0f, 0.0f};
        return restart;
    }
    if (event.kind != EventKind::OutOfPlay) {
        return std::nullopt;
    }

    const float ySign = ball.y < 0.0f ? -1.0f : 1.0f;
    switch (event.line) {
    case physics::Line::TouchlineMinusY:
    case physics::Line::TouchlinePlusY:
        restart.kind = RestartKind::ThrowIn;
        restart.side = physics::opposite(event.side);
        restart.spot = {std::clamp(ball.x, -pitch::kHalfLength + tune::kInset, pitch::kHalfLength - tune::kInset), ySign * (pitch::kHalfWidth - tune::kInset)};
        return restart;
    case physics::Line::GoalLineMinusX:
    case physics::Line::GoalLinePlusX: {
        const Side end = event.line == physics::Line::GoalLineMinusX ? Side::Home : Side::Away;
        const float inward = end == Side::Home ? 1.0f : -1.0f;
        if (event.side == end) {
            restart.kind = RestartKind::Corner;
            restart.side = physics::opposite(end);
            restart.spot = {pitch::goalLineX(end) + inward * tune::kInset, ySign * (pitch::kHalfWidth - tune::kInset)};
        } else {
            restart.kind = RestartKind::GoalKick;
            restart.side = end;
            restart.spot = {pitch::goalLineX(end) + inward * tune::kGoalKickDepth, ySign * tune::kGoalKickOffset};
        }
        return restart;
    }
    default:
        return std::nullopt;
    }
}

void arrange(Player* players, std::size_t count, Restart& restart)
{
    restart.taker = pickTaker(players, count, restart);
    if (restart.taker >= count) {
        return;
    }
    Player& taker = players[restart.taker];
    const float attack = attackSign(restart.side);

    if (restart.kind == RestartKind::ThrowIn) {
        const float out = restart.spot.y < 0.0f ? -1.0f : 1.0f;
        taker.position = {restart.spot.x, restart.spot.y + out * tune::kTakerDistance};
        taker.facing = out < 0.0f ? dmath::kHalfPi : -dmath::kHalfPi;
    } else {
        taker.position = {restart.spot.x - attack * tune::kTakerDistance, restart.spot.y};
        const Vec2 toGoal = restart.kind == RestartKind::Corner ? physics::pitch::goalCentre(physics::opposite(restart.side)) - restart.spot : Vec2{attack, 0.0f};
        taker.facing = dmath::atan2(toGoal.y, toGoal.x);
    }
    taker.velocity = {};
    taker.cooldown = 0;

    if (restart.kind == RestartKind::Penalty) {
        const Side defending = physics::opposite(restart.side);
        Player& keeper = players[firstIndex(defending)];
        if (!keeper.sentOff) {
            keeper.position = {physics::pitch::goalLineX(defending) + attack * tune::kKeeperDepth, 0.0f};
            keeper.velocity = {};
        }
    }
    keepClear(players, count, restart);
}

void keepClear(Player* players, std::size_t count, const Restart& restart)
{
    const std::size_t defendingKeeper = firstIndex(physics::opposite(restart.side));
    for (std::size_t i = 0; i < count; ++i) {
        Player& p = players[i];
        if (p.sentOff || p.id == restart.taker) {
            continue;
        }
        switch (restart.kind) {
        case RestartKind::Kickoff:
            clearKickoff(p, restart);
            break;
        case RestartKind::Penalty:
            if (i != defendingKeeper) {
                clearPenalty(p, restart);
            }
            break;
        case RestartKind::ThrowIn:
            if (p.side != restart.side) {
                pushOut(p, restart.spot, tune::kThrowClearRadius);
            }
            break;
        case RestartKind::GoalKick:
            break;
        default:
            if (p.side != restart.side) {
                pushOut(p, restart.spot, tune::kClearRadius);
            }
            break;
        }
    }
}

ActionRequest shapeRequest(const Restart& restart, ActionRequest request)
{
    if (request.kind == ActionKind::None) {
        return request;
    }
    if (restart.kind == RestartKind::ThrowIn) {
        request.kind = ActionKind::Pass;
    } else if (restart.kind == RestartKind::Corner && (request.kind == ActionKind::Pass || request.kind == ActionKind::Through)) {
        request.kind = ActionKind::Cross;
    }
    return request;
}

void shapeKick(const Restart& restart, Kick& kick, physics::Vec3& ballPosition)
{
    if (restart.kind != RestartKind::ThrowIn) {
        return;
    }
    const Vec2 flat = physics::xy(kick.velocity);
    const float speed = physics::length(flat);
    const Vec2 capped = speed > tune::kThrowSpeedMax ? flat * (tune::kThrowSpeedMax / speed) : flat;
    kick.velocity = {capped.x, capped.y, tune::kThrowLift};
    kick.spin = {};
    ballPosition.z = tune::kThrowReleaseHeight;
}

bool scoresIndirectly(RestartKind kind) { return kind == RestartKind::IndirectFreeKick || kind == RestartKind::ThrowIn; }

bool exemptFromOffside(RestartKind kind)
{
    return kind == RestartKind::ThrowIn || kind == RestartKind::GoalKick || kind == RestartKind::Corner;
}

}

}

#include "nutmeg/ai/team_shape.h"

#include <algorithm>

namespace nutmeg::ai {

namespace {

using physics::Side;
using physics::Vec2;
namespace pitch = physics::pitch;

constexpr float kMaxX = pitch::kHalfLength - tuning::kPitchInset;
constexpr float kMaxY = pitch::kHalfWidth - tuning::kPitchInset;
constexpr Vec2 kOwnGoal = {-pitch::kHalfLength, 0.0f};

// Away is the Home frame turned half a revolution about the centre spot, the
// same mirroring World uses, and negation is exact in floating point.
constexpr float signOf(Side side) { return side == Side::Home ? 1.0f : -1.0f; }

constexpr Vec2 flip(Vec2 v, float sign) { return {v.x * sign, v.y * sign}; }

Vec2 insidePitch(Vec2 p)
{
    return {std::clamp(p.x, -kMaxX, kMaxX), std::clamp(p.y, -kMaxY, kMaxY)};
}

Vec2 towardsOwnGoal(Vec2 from, float length)
{
    const Vec2 d = kOwnGoal - from;
    const float len = physics::length(d);
    if (len < 1e-6f) {
        return from;
    }
    return from + d * (length / len);
}

float urgencyHolding(Vec2 position, Vec2 target)
{
    const float far = std::min(physics::distance(position, target) / tuning::kCatchUpDistance, 1.0f);
    return far * tuning::kHoldMaxUrgency;
}

std::array<Vec2, kOutfielders> shapeTargets(const TeamInput& in, Phase phase, Vec2 ball, std::optional<float> offside)
{
    const auto& formation = slots(in.formation);
    const float shift = std::clamp(
        tuning::kBallFollowX * ball.x + tuning::lineHeight(in.mentality) + tuning::phaseShift(phase),
        tuning::kMinShiftX, tuning::kMaxShiftX);
    const float lateral = std::clamp(tuning::kBallFollowY * ball.y, -tuning::kMaxShiftY, tuning::kMaxShiftY);
    const float depth = tuning::depthScale(phase, in.mentality);
    const float width = tuning::widthScale(phase, in.mentality);

    std::array<Vec2, kOutfielders> targets{};
    for (std::size_t i = 0; i < kOutfielders; ++i) {
        const Vec2 anchor = formation[i].anchor;
        Vec2 p = {tuning::kBlockCentreX + (anchor.x - tuning::kBlockCentreX) * depth + shift, anchor.y * width + lateral};
        if (offside && phase != Phase::Defending && formation[i].line == Line::Attack) {
            p.x = std::min(p.x, std::max(*offside, ball.x) - tuning::kOffsideMargin);
        }
        targets[i] = insidePitch(p);
    }
    return targets;
}

std::size_t nearestTo(Vec2 point, const std::array<Vec2, kOutfielders>& points, const std::array<bool, kOutfielders>& skip)
{
    std::size_t best = kOutfielders;
    float bestDistance = 0.0f;
    for (std::size_t i = 0; i < kOutfielders; ++i) {
        if (skip[i]) {
            continue;
        }
        const float d = physics::distance(points[i], point);
        if (best == kOutfielders || d < bestDistance) {
            best = i;
            bestDistance = d;
        }
    }
    return best;
}

}

Phase phaseOf(Side side, BallControl control)
{
    if (control == BallControl::Loose) {
        return Phase::Loose;
    }
    const bool ours = (control == BallControl::Home) == (side == Side::Home);
    return ours ? Phase::Attacking : Phase::Defending;
}

TeamIntent planTeam(const TeamInput& in)
{
    const float sign = signOf(in.side);
    const Phase phase = phaseOf(in.side, in.control);
    const Vec2 ball = flip(in.ball, sign);

    std::array<Vec2, kOutfielders> positions{};
    std::array<Vec2, kOutfielders> opponents{};
    for (std::size_t i = 0; i < kOutfielders; ++i) {
        positions[i] = flip(in.positions[i], sign);
        opponents[i] = flip(in.opponents[i], sign);
    }
    std::optional<float> offside;
    if (in.offsideLineX) {
        offside = *in.offsideLineX * sign;
    }

    std::array<Vec2, kOutfielders> targets = shapeTargets(in, phase, ball, offside);
    TeamIntent out;
    out.phase = phase;
    out.roles.fill(Role::Hold);
    std::array<float, kOutfielders> urgency{};
    for (std::size_t i = 0; i < kOutfielders; ++i) {
        urgency[i] = urgencyHolding(positions[i], targets[i]);
    }

    const std::size_t pressers = phase == Phase::Defending ? 2 : phase == Phase::Loose ? 1 : 0;
    std::array<bool, kOutfielders> pressing{};
    const Vec2 pressPoint = insidePitch(towardsOwnGoal(ball, tuning::kPressStandOff));
    for (std::size_t n = 0; n < pressers; ++n) {
        const std::size_t i = nearestTo(ball, positions, pressing);
        const float range = n == 0 ? tuning::kPressRange : tuning::kSecondPresserRange;
        if (i == kOutfielders || physics::distance(positions[i], ball) > range) {
            break;
        }
        pressing[i] = true;
        out.roles[i] = Role::Press;
        targets[i] = pressPoint;
        urgency[i] = n == 0 ? tuning::kPressUrgency : tuning::kSecondPressUrgency;
    }

    if (phase == Phase::Defending) {
        std::array<bool, kOutfielders> unavailable{};
        for (std::size_t j = 0; j < kOutfielders; ++j) {
            unavailable[j] = physics::distance(opponents[j], ball) < tuning::kBallCarrierRadius;
        }
        const auto& formation = slots(in.formation);
        for (std::size_t i = 0; i < kOutfielders; ++i) {
            if (pressing[i] || formation[i].line == Line::Attack) {
                continue;
            }
            const std::size_t j = nearestTo(targets[i], opponents, unavailable);
            if (j == kOutfielders || physics::distance(opponents[j], targets[i]) > tuning::kMarkRadius) {
                continue;
            }
            unavailable[j] = true;
            const Vec2 markPoint = towardsOwnGoal(opponents[j], tuning::kMarkGoalSide);
            targets[i] = insidePitch(targets[i] + (markPoint - targets[i]) * tuning::kMarkBlend);
            urgency[i] = urgencyHolding(positions[i], targets[i]);
            out.roles[i] = Role::Mark;
        }
    }

    for (std::size_t i = 0; i < kOutfielders; ++i) {
        out.targets[i] = {flip(targets[i], sign), urgency[i]};
    }
    return out;
}

ShapeMetrics measure(Side side, Formation formation, const std::array<Vec2, kOutfielders>& points)
{
    const float sign = signOf(side);
    const auto& layout = slots(formation);
    float minX = 0.0f, maxX = 0.0f, minY = 0.0f, maxY = 0.0f, defenceSum = 0.0f;
    int defenders = 0;
    for (std::size_t i = 0; i < kOutfielders; ++i) {
        const Vec2 p = flip(points[i], sign);
        if (i == 0 || p.x < minX) minX = p.x;
        if (i == 0 || p.x > maxX) maxX = p.x;
        if (i == 0 || p.y < minY) minY = p.y;
        if (i == 0 || p.y > maxY) maxY = p.y;
        if (layout[i].line == Line::Defence) {
            defenceSum += p.x;
            ++defenders;
        }
    }
    ShapeMetrics m;
    m.lineHeight = defenders > 0 ? defenceSum / static_cast<float>(defenders) : 0.0f;
    m.blockLength = maxX - minX;
    m.blockWidth = maxY - minY;
    return m;
}

}

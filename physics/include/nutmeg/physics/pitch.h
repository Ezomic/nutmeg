#pragma once

#include "nutmeg/physics/vec.h"

namespace nutmeg::physics {

// The two teams. Home defends the goal at -x and attacks +x; Away the reverse.
enum class Side { Home, Away };

constexpr Side opposite(Side s) { return s == Side::Home ? Side::Away : Side::Home; }

// Origin at the centre spot. Dimensions are the IFAB Laws of the Game
// measurements for a 105 x 68 m pitch.
namespace pitch {

constexpr float kLength = 105.0f;
constexpr float kWidth = 68.0f;
constexpr float kHalfLength = kLength * 0.5f;
constexpr float kHalfWidth = kWidth * 0.5f;

constexpr float kLineWidth = 0.12f;
constexpr float kCentreCircleRadius = 9.15f;
constexpr float kCornerArcRadius = 1.0f;

constexpr float kGoalWidth = 7.32f;
constexpr float kGoalHeight = 2.44f;
constexpr float kGoalDepth = 2.0f;
constexpr float kPostRadius = 0.06f;
constexpr float kHalfGoalWidth = kGoalWidth * 0.5f;

constexpr float kPenaltyAreaDepth = 16.5f;
constexpr float kPenaltyAreaWidth = 40.32f;
constexpr float kGoalAreaDepth = 5.5f;
constexpr float kGoalAreaWidth = 18.32f;
constexpr float kPenaltySpotDistance = 11.0f;
constexpr float kPenaltyArcRadius = 9.15f;

constexpr float goalLineX(Side defending) { return defending == Side::Home ? -kHalfLength : kHalfLength; }

constexpr Vec2 penaltySpot(Side defending)
{
    const float sign = defending == Side::Home ? 1.0f : -1.0f;
    return {goalLineX(defending) + sign * kPenaltySpotDistance, 0.0f};
}

constexpr Vec2 goalCentre(Side defending) { return {goalLineX(defending), 0.0f}; }

constexpr Vec2 postMinusY(Side defending) { return {goalLineX(defending), -kHalfGoalWidth}; }
constexpr Vec2 postPlusY(Side defending) { return {goalLineX(defending), kHalfGoalWidth}; }

constexpr bool contains(Vec2 p)
{
    return p.x >= -kHalfLength && p.x <= kHalfLength && p.y >= -kHalfWidth && p.y <= kHalfWidth;
}

constexpr bool inBoxAt(Vec2 p, Side defending, float depth, float width)
{
    const float intoPitch = defending == Side::Home ? p.x - goalLineX(defending) : goalLineX(defending) - p.x;
    return intoPitch >= 0.0f && intoPitch <= depth && p.y >= -width * 0.5f && p.y <= width * 0.5f;
}

constexpr bool inPenaltyArea(Vec2 p, Side defending)
{
    return inBoxAt(p, defending, kPenaltyAreaDepth, kPenaltyAreaWidth);
}

constexpr bool inGoalArea(Vec2 p, Side defending)
{
    return inBoxAt(p, defending, kGoalAreaDepth, kGoalAreaWidth);
}

constexpr bool inCentreCircle(Vec2 p) { return dot(p, p) <= kCentreCircleRadius * kCentreCircleRadius; }

constexpr Side halfOf(Vec2 p) { return p.x < 0.0f ? Side::Home : Side::Away; }

}

}

#pragma once

namespace nutmeg::ai {

enum class Mentality { Attacking, Balanced, Defensive };
enum class Phase { Attacking, Defending, Loose };

// Every number the team shape reads. Metres unless noted. "Local" means the
// frame where the team attacks +x, so the same values serve both sides.
namespace tuning {

constexpr float kPitchInset = 2.0f;

constexpr float kBlockCentreX = -20.0f;

constexpr float kBallFollowX = 0.3f;
constexpr float kBallFollowY = 0.2f;
constexpr float kMinShiftX = -14.0f;
constexpr float kMaxShiftX = 22.0f;
constexpr float kMaxShiftY = 8.0f;

constexpr float lineHeight(Mentality m)
{
    switch (m) {
    case Mentality::Attacking: return 8.0f;
    case Mentality::Defensive: return -8.0f;
    case Mentality::Balanced: break;
    }
    return 0.0f;
}

constexpr float phaseShift(Phase p)
{
    switch (p) {
    case Phase::Attacking: return 5.0f;
    case Phase::Defending: return -4.0f;
    case Phase::Loose: break;
    }
    return 0.0f;
}

// Multiplies the spread of the lines around kBlockCentreX.
constexpr float depthScale(Phase p, Mentality m)
{
    float scale = 1.0f;
    switch (p) {
    case Phase::Attacking: scale = 1.15f; break;
    case Phase::Defending: scale = 0.8f; break;
    case Phase::Loose: break;
    }
    return m == Mentality::Defensive ? scale * 0.92f : scale;
}

constexpr float widthScale(Phase p, Mentality m)
{
    float scale = 1.0f;
    switch (p) {
    case Phase::Attacking: scale = 1.15f; break;
    case Phase::Defending: scale = 0.85f; break;
    case Phase::Loose: break;
    }
    return m == Mentality::Attacking ? scale * 1.05f : scale;
}

constexpr float kOffsideMargin = 0.5f;

constexpr float kPressRange = 30.0f;
constexpr float kSecondPresserRange = 14.0f;
constexpr float kPressStandOff = 1.2f;

constexpr float kMarkRadius = 8.0f;
constexpr float kMarkGoalSide = 2.5f;
constexpr float kMarkBlend = 0.6f;
constexpr float kBallCarrierRadius = 2.0f;

// Urgency runs 0 (stroll) to 1 (flat out). Players holding shape never
// exceed kHoldMaxUrgency, pressers run at kPressUrgency and kSecondPressUrgency.
constexpr float kPressUrgency = 1.0f;
constexpr float kSecondPressUrgency = 0.85f;
constexpr float kHoldMaxUrgency = 0.6f;
constexpr float kCatchUpDistance = 25.0f;

constexpr float kSprintUrgency = 0.75f;
constexpr float kArriveRadius = 1.5f;

}

}

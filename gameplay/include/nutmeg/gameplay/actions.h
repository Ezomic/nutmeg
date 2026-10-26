#pragma once

#include "nutmeg/core/rng.h"
#include "nutmeg/gameplay/locomotion.h"
#include "nutmeg/gameplay/player.h"
#include "nutmeg/physics/ball.h"
#include "nutmeg/physics/vec.h"

#include <cstdint>

namespace nutmeg::gameplay {

inline constexpr std::uint32_t kNoPlayer = 0xFFFFFFFFu;

enum class ActionKind : std::uint8_t { None, Pass, Through, Shoot, Cross, Tackle };

// What a player wants to do with the ball this tick. Human input is turned
// into these, and the AI fills them in directly.
struct ActionRequest {
    ActionKind kind = ActionKind::None;
    // A point on the pitch. For a pass it is where to play the ball, for a shot
    // where to aim it. Ignored when `target` names a teammate.
    physics::Vec2 aim;
    std::uint32_t target = kNoPlayer;
    // 0..1. Sets the launch speed of a pass, through ball or shot. A cross
    // ignores it: its range is the distance to the aim point.
    float power = 1.0f;
    // Shots only: a chip instead of a drive.
    bool lofted = false;

    bool operator==(const ActionRequest&) const = default;
};

// Tuning for possession and the ball actions. Distances are metres, speeds
// m/s, angles radians, times ticks. Skill terms map an attribute 1..100 to 0..1.
namespace possession {

constexpr float kBodyContactRadius = locomotion::kBodyRadius + physics::ball::kRadius;
constexpr float kControlRadius = 0.75f;
constexpr float kControlHeight = 0.5f;
constexpr float kDeflectHeight = 1.9f;
constexpr float kDeflectRestitution = 0.5f;

// A ball arriving faster than this relative to the player cannot be trapped.
constexpr float kTrapSpeedBase = 6.0f;
constexpr float kTrapSpeedSkill = 10.0f;
constexpr float kHeavyTouchKeep = 0.35f;
constexpr float kHeavyTouchScatter = 0.6f;
constexpr int kHeavyTouchCooldown = 15;

constexpr float kContestNoise = 0.25f;
constexpr float kContestDistanceWeight = 0.3f;

constexpr float kCarryBase = 0.45f;
constexpr float kCarrySpeedLead = 0.04f;
constexpr float kCarryDribbleTighten = 0.5f;
constexpr float kCarryMinSpeed = 0.5f;

constexpr float kKickReach = 1.0f;
constexpr float kKickHeight = 0.6f;
constexpr int kKickCooldown = 18;

constexpr float kPassSpeedMin = 4.0f;
constexpr float kPassSpeedMax = 26.0f;
constexpr float kPassErrorBest = 0.02f;
constexpr float kPassErrorWorst = 0.14f;
constexpr float kThroughErrorFactor = 1.3f;
constexpr float kPassArrivalSpeed = 3.5f;
constexpr int kLeadIterations = 3;

constexpr float kCrossLoftSin = 0.5736f;
constexpr float kCrossLoftCos = 0.8192f;
constexpr float kCrossLoftSin2 = 0.9397f;
constexpr float kCrossRangeFactor = 1.06f;
constexpr float kCrossErrorBest = 0.03f;
constexpr float kCrossErrorWorst = 0.18f;

constexpr float kShotSpeedMin = 12.0f;
constexpr float kShotSpeedMax = 32.0f;
constexpr float kShotErrorBest = 0.015f;
constexpr float kShotErrorWorst = 0.12f;
constexpr float kShotFinishingWeight = 0.6f;
constexpr float kShotLoftSin = 0.342f;
constexpr float kShotLoftCos = 0.940f;
constexpr float kShotAimFraction = 0.8f;

constexpr float kPressureRadius = 3.0f;
constexpr float kPressureErrorGain = 1.0f;
constexpr float kComposureShield = 0.5f;

constexpr float kTackleReach = 1.3f;
constexpr float kTackleBase = 0.5f;
constexpr float kTackleSlope = 0.8f;
constexpr float kTackleMin = 0.1f;
constexpr float kTackleMax = 0.9f;
constexpr float kTackleKnock = 5.0f;
constexpr int kTackleFailCooldown = 30;
constexpr int kTackledCooldown = 20;
constexpr float kFoulBase = 0.05f;
constexpr float kFoulAggression = 0.35f;
constexpr float kFoulOnWinScale = 0.3f;

constexpr float kAssistCosine = 0.82f;
constexpr float kAssistMinDistance = 3.0f;
constexpr float kAssistMaxDistance = 45.0f;
constexpr float kAssistDistanceWeight = 0.01f;

}

struct Kick {
    physics::Vec3 velocity;
    physics::Vec3 spin;
};

// Attribute rating 1..100 as 0..1.
float skill(int rating);

// 0 with no opponent within kPressureRadius, 1 with one on top of the kicker.
float pressureOn(const Player& kicker, const Player* players, std::size_t count);

float passSpeed(float power);
// Metres a ball kicked along the ground at `speed` rolls before it stops.
float rollDistance(float speed);
// The power that makes a ground ball arrive `distance` away at `arrivalSpeed`.
float passPowerForDistance(float distance, float arrivalSpeed = possession::kPassArrivalSpeed);
// Where to aim a through ball so it meets `receiver` running at its current velocity.
physics::Vec2 leadPoint(physics::Vec2 from, const Player& receiver, float launchSpeed);

// `aim` is where the kicker means the ball to go. The noise comes from `rng`.
Kick passKick(const Player& kicker, physics::Vec2 aim, float power, float pressure, bool through, Rng& rng);
Kick crossKick(const Player& kicker, physics::Vec2 aim, float pressure, Rng& rng);
Kick shotKick(const Player& kicker, physics::Vec2 aim, float power, float pressure, bool lofted, Rng& rng);

// Chance that `tackler` takes the ball off `carrier`, and the chance the
// attempt is a foul candidate.
float tackleSuccessChance(const Player& tackler, const Player& carrier);
float foulChance(const Player& tackler, bool won);

}

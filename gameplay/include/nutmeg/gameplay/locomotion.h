#pragma once

#include "nutmeg/physics/vec.h"

namespace nutmeg::gameplay {

struct Player;

// Tuning for player movement. Speeds are m/s, accelerations m/s^2, turn rates
// rad/s. Pace 1..100 interpolates linearly between the slow and fast ends.
namespace locomotion {

constexpr float kStepSeconds = 1.0f / 60.0f;

constexpr float kSprintSpeedSlow = 7.0f;
constexpr float kSprintSpeedFast = 10.0f;
constexpr float kRunFractionOfSprint = 0.7f;

constexpr float kAccelerationSlow = 4.5f;
constexpr float kAccelerationFast = 6.5f;
constexpr float kBrakeFactor = 1.5f;

constexpr float kTurnRateStanding = 12.0f;
constexpr float kTurnRateAtSprint = 3.0f;
constexpr float kFacingDeadzone = 0.1f;

constexpr float kBodyRadius = 0.35f;
constexpr float kPitchMargin = 3.0f;
constexpr int kSeparationPasses = 3;

float sprintSpeed(int pace);
float runSpeed(int pace);
float acceleration(int pace);
float turnRate(float speed, int pace);

// `intent` is the stick in world axes, length at most 1.
void step(Player& player, physics::Vec2 intent, bool sprint);

}

}

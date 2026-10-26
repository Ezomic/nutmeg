#include "nutmeg/gameplay/locomotion.h"

#include "nutmeg/core/dmath.h"
#include "nutmeg/gameplay/player.h"

namespace nutmeg::gameplay::locomotion {

namespace {

float paceFraction(int pace)
{
    const int clamped = pace < schema::minRating ? schema::minRating : (pace > schema::maxRating ? schema::maxRating : pace);
    return static_cast<float>(clamped - schema::minRating) / static_cast<float>(schema::maxRating - schema::minRating);
}

float lerp(float a, float b, float t) { return a + (b - a) * t; }

float wrapAngle(float a)
{
    return a - std::floor(a * dmath::kInvTwoPi + 0.5f) * dmath::kTwoPi;
}

void turn(Player& player, physics::Vec2 intent, float speed)
{
    if (physics::length(intent) < kFacingDeadzone) {
        return;
    }
    const float target = dmath::atan2(intent.y, intent.x);
    const float diff = wrapAngle(target - player.facing);
    const float maxTurn = turnRate(speed, player.profile.attributes.pace) * kStepSeconds;
    const float applied = diff > maxTurn ? maxTurn : (diff < -maxTurn ? -maxTurn : diff);
    player.facing = wrapAngle(player.facing + applied);
}

}

float sprintSpeed(int pace) { return lerp(kSprintSpeedSlow, kSprintSpeedFast, paceFraction(pace)); }

float runSpeed(int pace) { return sprintSpeed(pace) * kRunFractionOfSprint; }

float acceleration(int pace) { return lerp(kAccelerationSlow, kAccelerationFast, paceFraction(pace)); }

float turnRate(float speed, int pace)
{
    float t = speed / sprintSpeed(pace);
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    return lerp(kTurnRateStanding, kTurnRateAtSprint, t);
}

void step(Player& player, physics::Vec2 intent, bool sprint)
{
    const int pace = player.profile.attributes.pace;
    const float magnitude = physics::length(intent);
    if (magnitude > 1.0f) {
        intent = intent * (1.0f / magnitude);
    }

    const physics::Vec2 desired = intent * (sprint ? sprintSpeed(pace) : runSpeed(pace));
    const physics::Vec2 delta = desired - player.velocity;
    const float deltaLength = physics::length(delta);
    const bool braking = physics::length(desired) < physics::length(player.velocity);
    const float maxDelta = acceleration(pace) * (braking ? kBrakeFactor : 1.0f) * kStepSeconds;
    player.velocity = deltaLength > maxDelta ? player.velocity + delta * (maxDelta / deltaLength) : desired;

    turn(player, intent, physics::length(player.velocity));

    player.position = player.position + player.velocity * kStepSeconds;
}

}

#include "nutmeg/gameplay/actions.h"

#include "nutmeg/core/dmath.h"

#include <cstddef>

namespace nutmeg::gameplay {

namespace {

using physics::Vec2;
using physics::Vec3;
namespace tune = possession;

float lerp(float a, float b, float t) { return a + (b - a) * t; }

float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

// Triangular noise in (-1, 1): two draws, always in this order.
float symmetric(Rng& rng)
{
    const float a = rng.nextFloat();
    const float b = rng.nextFloat();
    return a + b - 1.0f;
}

Vec2 facingOf(const Player& p) { return {dmath::cos(p.facing), dmath::sin(p.facing)}; }

Vec2 directionTo(const Player& kicker, Vec2 aim)
{
    const Vec2 d = aim - kicker.position;
    const float len = physics::length(d);
    return len > 1e-4f ? d * (1.0f / len) : facingOf(kicker);
}

Vec2 rotate(Vec2 v, float angle)
{
    const float c = dmath::cos(angle);
    const float s = dmath::sin(angle);
    return {v.x * c - v.y * s, v.x * s + v.y * c};
}

float pressureError(float pressure, float shield)
{
    return 1.0f + tune::kPressureErrorGain * pressure * (1.0f - shield);
}

// One tick of the ball rolling on grass, as Ball::step does it.
float rollStep(float speed)
{
    constexpr float dt = physics::ball::kStepSeconds;
    speed -= physics::ball::kRollDecel * dt;
    if (speed <= 0.0f) {
        return 0.0f;
    }
    speed -= physics::ball::kDragPerMetre * speed * speed * dt;
    return speed > 0.0f ? speed : 0.0f;
}

float rollTime(float speed, float distance)
{
    constexpr float dt = physics::ball::kStepSeconds;
    float travelled = 0.0f;
    float time = 0.0f;
    while (speed > 0.0f && travelled < distance) {
        speed = rollStep(speed);
        travelled += speed * dt;
        time += dt;
    }
    return time;
}

}

float skill(int rating)
{
    return static_cast<float>(schema::clampRating(rating) - schema::minRating) / static_cast<float>(schema::maxRating - schema::minRating);
}

float pressureOn(const Player& kicker, const Player* players, std::size_t count)
{
    float nearest = tune::kPressureRadius;
    for (std::size_t i = 0; i < count; ++i) {
        if (players[i].side == kicker.side) {
            continue;
        }
        const float d = physics::distance(players[i].position, kicker.position);
        nearest = d < nearest ? d : nearest;
    }
    return 1.0f - nearest / tune::kPressureRadius;
}

float passSpeed(float power) { return lerp(tune::kPassSpeedMin, tune::kPassSpeedMax, clamp01(power)); }

float rollDistance(float speed)
{
    constexpr float dt = physics::ball::kStepSeconds;
    float travelled = 0.0f;
    while (speed > 0.0f) {
        speed = rollStep(speed);
        travelled += speed * dt;
    }
    return travelled;
}

float passPowerForDistance(float distance, float arrivalSpeed)
{
    constexpr float dt = physics::ball::kStepSeconds;
    float lo = arrivalSpeed;
    float hi = tune::kPassSpeedMax;
    for (int i = 0; i < 24; ++i) {
        const float mid = 0.5f * (lo + hi);
        float speed = mid;
        float travelled = 0.0f;
        while (speed > arrivalSpeed) {
            speed = rollStep(speed);
            travelled += speed * dt;
        }
        (travelled < distance ? lo : hi) = mid;
    }
    const float speed = 0.5f * (lo + hi);
    return clamp01((speed - tune::kPassSpeedMin) / (tune::kPassSpeedMax - tune::kPassSpeedMin));
}

Vec2 leadPoint(Vec2 from, const Player& receiver, float launchSpeed)
{
    Vec2 point = receiver.position;
    for (int i = 0; i < tune::kLeadIterations; ++i) {
        const float t = rollTime(launchSpeed, physics::distance(from, point));
        point = receiver.position + receiver.velocity * t;
    }
    return point;
}

Kick passKick(const Player& kicker, Vec2 aim, float power, float pressure, bool through, Rng& rng)
{
    const float base = lerp(tune::kPassErrorWorst, tune::kPassErrorBest, skill(kicker.profile.attributes.passing));
    const float error = base * (through ? tune::kThroughErrorFactor : 1.0f) * (0.6f + 0.4f * clamp01(power)) * pressureError(pressure, 0.0f);
    const float angle = symmetric(rng) * error;
    const float speedNoise = symmetric(rng) * error * 0.5f;

    const Vec2 dir = rotate(directionTo(kicker, aim), angle);
    const float speed = passSpeed(power) * (1.0f + speedNoise);
    return {{dir.x * speed, dir.y * speed, 0.0f}, {}};
}

Kick crossKick(const Player& kicker, Vec2 aim, float pressure, Rng& rng)
{
    const float error = lerp(tune::kCrossErrorWorst, tune::kCrossErrorBest, skill(kicker.profile.attributes.crossing)) * pressureError(pressure, 0.0f);
    const float angle = symmetric(rng) * error;
    const float rangeNoise = symmetric(rng) * error;

    const float range = physics::distance(aim, kicker.position) * tune::kCrossRangeFactor * (1.0f + rangeNoise);
    const float speed = std::sqrt(range * physics::ball::kGravity / tune::kCrossLoftSin2);
    const Vec2 dir = rotate(directionTo(kicker, aim), angle);
    const float flat = speed * tune::kCrossLoftCos;
    return {{dir.x * flat, dir.y * flat, speed * tune::kCrossLoftSin}, {}};
}

Kick shotKick(const Player& kicker, Vec2 aim, float power, float pressure, bool lofted, Rng& rng)
{
    const auto& a = kicker.profile.attributes;
    const float composure = skill(a.composure);
    const float finishing = lerp(composure, skill(a.finishing), tune::kShotFinishingWeight);
    const float error = lerp(tune::kShotErrorWorst, tune::kShotErrorBest, finishing) * (0.7f + 0.6f * clamp01(power)) * pressureError(pressure, tune::kComposureShield * composure);
    const float sideways = symmetric(rng) * error;
    const float vertical = symmetric(rng) * error;

    const Vec2 dir = rotate(directionTo(kicker, aim), sideways);
    const float speed = lerp(tune::kShotSpeedMin, tune::kShotSpeedMax, clamp01(power));
    float sinElevation;
    float cosElevation;
    if (lofted) {
        const float s = tune::kShotLoftSin + vertical * 0.5f;
        sinElevation = s < 0.0f ? 0.0f : s;
        cosElevation = std::sqrt(1.0f - sinElevation * sinElevation);
    } else {
        const float elevation = (vertical < 0.0f ? -vertical : vertical) * 0.8f;
        sinElevation = dmath::sin(elevation);
        cosElevation = dmath::cos(elevation);
    }
    const float flat = speed * cosElevation;
    return {{dir.x * flat, dir.y * flat, speed * sinElevation}, {}};
}

float tackleSuccessChance(const Player& tackler, const Player& carrier)
{
    const auto& t = tackler.profile.attributes;
    const auto& c = carrier.profile.attributes;
    const float attack = 0.7f * skill(t.tackling) + 0.3f * skill(t.strength);
    const float defence = 0.7f * skill(c.dribbling) + 0.3f * skill(c.strength);
    const float p = tune::kTackleBase + (attack - defence) * tune::kTackleSlope;
    return p < tune::kTackleMin ? tune::kTackleMin : (p > tune::kTackleMax ? tune::kTackleMax : p);
}

float foulChance(const Player& tackler, bool won)
{
    const float p = tune::kFoulBase + tune::kFoulAggression * skill(tackler.profile.attributes.aggression);
    return won ? p * tune::kFoulOnWinScale : p;
}

}

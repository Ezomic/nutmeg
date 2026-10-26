#include "nutmeg/physics/ball.h"

#include "nutmeg/core/hash.h"

#include <bit>
#include <cmath>
#include <optional>

namespace nutmeg::physics {

BallStatus classify(Vec3 c)
{
    constexpr float r = ball::kRadius;

    for (const Side defending : {Side::Home, Side::Away}) {
        const float sign = defending == Side::Home ? -1.0f : 1.0f;
        if (sign * c.x - r <= pitch::kHalfLength) {
            continue;
        }
        const Line line = defending == Side::Home ? Line::GoalLineMinusX : Line::GoalLinePlusX;
        const bool betweenPosts = std::abs(c.y) + r <= pitch::kHalfGoalWidth;
        const bool underBar = c.z + r <= pitch::kGoalHeight;
        if (betweenPosts && underBar) {
            return {PlayStatus::Goal, line, opposite(defending)};
        }
        return {PlayStatus::OutOfPlay, line, Side::Home};
    }

    if (c.y - r > pitch::kHalfWidth) {
        return {PlayStatus::OutOfPlay, Line::TouchlinePlusY, Side::Home};
    }
    if (-c.y - r > pitch::kHalfWidth) {
        return {PlayStatus::OutOfPlay, Line::TouchlineMinusY, Side::Home};
    }
    return {};
}

namespace {

struct Contact {
    float t = 0.0f;
    Vec3 normal;
};

struct Circle {
    float t = 0.0f;
    Vec2 normal;
};

// A start already inside only counts when moving inward.
std::optional<Circle> sweepCircle(Vec2 p, Vec2 d, Vec2 c, float r)
{
    const Vec2 f = p - c;
    const float a = dot(d, d);
    const float b = dot(f, d);
    const float k = dot(f, f) - r * r;
    if (a == 0.0f || b >= 0.0f) {
        return std::nullopt;
    }
    if (k < 0.0f) {
        return Circle{0.0f, f * (1.0f / std::sqrt(dot(f, f)))};
    }
    const float disc = b * b - a * k;
    if (disc < 0.0f) {
        return std::nullopt;
    }
    const float t = (-b - std::sqrt(disc)) / a;
    if (t > 1.0f) {
        return std::nullopt;
    }
    return Circle{t, (f + d * t) * (1.0f / r)};
}

// The ball centre against the posts and crossbar, each grown by the ball radius.
std::optional<Contact> sweepGoals(Vec3 p, Vec3 d)
{
    constexpr float reach = ball::kRadius + pitch::kPostRadius;
    std::optional<Contact> best;
    const auto keep = [&best](float t, Vec3 normal) {
        if (!best || t < best->t) {
            best = Contact{t, normal};
        }
    };

    for (const float end : {-1.0f, 1.0f}) {
        if (end * p.x < pitch::kHalfLength - 1.0f && end * (p.x + d.x) < pitch::kHalfLength - 1.0f) {
            continue;
        }
        const float goalLine = end * pitch::kHalfLength;

        for (const float side : {-1.0f, 1.0f}) {
            const auto hit = sweepCircle(xy(p), xy(d), {goalLine, side * pitch::kHalfGoalWidth}, reach);
            if (!hit) continue;
            const float z = p.z + d.z * hit->t;
            if (z >= 0.0f && z <= pitch::kGoalHeight) {
                keep(hit->t, {hit->normal.x, hit->normal.y, 0.0f});
            }
        }

        const auto bar = sweepCircle({p.x, p.z}, {d.x, d.z}, {goalLine, pitch::kGoalHeight}, reach);
        if (bar && std::abs(p.y + d.y * bar->t) <= pitch::kHalfGoalWidth) {
            keep(bar->t, {bar->normal.x, 0.0f, bar->normal.y});
        }
    }
    return best;
}

constexpr float kContactSkin = 1e-4f;
constexpr int kMaxContactsPerStep = 2;

}

void Ball::step(const Weather& weather)
{
    constexpr float dt = ball::kStepSeconds;
    constexpr float r = ball::kRadius;

    const bool onGround = grounded();
    const Vec3 wind{weather.wind.x, weather.wind.y, 0.0f};

    if (onGround) {
        const float planar = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
        const float slow = ball::kRollDecel * (1.0f - weather::kWetRollDecelReduction * weather.wetPitch) * dt;
        if (planar <= slow) {
            velocity.x = 0.0f;
            velocity.y = 0.0f;
        } else {
            const float keep = (planar - slow) / planar;
            velocity.x *= keep;
            velocity.y *= keep;
        }
        spin = spin * (1.0f - ball::kGroundSpinDecayPerSecond * dt);
    } else {
        spin = spin * (1.0f - ball::kSpinDecayPerSecond * dt);
    }

    const Vec3 air = velocity - wind;
    const float speed = length(air);
    Vec3 accel{0.0f, 0.0f, onGround ? 0.0f : -ball::kGravity};
    accel = accel - air * (ball::kDragPerMetre * speed);
    if (!onGround) {
        accel = accel + cross(spin, air) * ball::kMagnus;
    }
    const bool atRest = velocity.x == 0.0f && velocity.y == 0.0f;
    if (onGround && atRest && (accel.x != 0.0f || accel.y != 0.0f)) {
        const float staticLimit = ball::kRollDecel * (1.0f - weather::kWetRollDecelReduction * weather.wetPitch);
        if (accel.x * accel.x + accel.y * accel.y <= staticLimit * staticLimit) {
            accel.x = 0.0f;
            accel.y = 0.0f;
        }
    }

    velocity = velocity + accel * dt;

    Vec3 remaining = velocity * dt;
    for (int i = 0; i < kMaxContactsPerStep; ++i) {
        const auto contact = sweepGoals(position, remaining);
        if (!contact) {
            position = position + remaining;
            break;
        }
        position = position + remaining * contact->t + contact->normal * kContactSkin;
        velocity = velocity - contact->normal * ((1.0f + ball::kPostRestitution) * dot(velocity, contact->normal));
        spin = spin * ball::kBounceSpinKeep;
        remaining = velocity * (dt * (1.0f - contact->t));
    }

    if (position.z < r) {
        position.z = r;
        if (velocity.z < 0.0f) {
            velocity.z = -velocity.z * (ball::kRestitution * (1.0f - weather::kRainRestitutionLoss * weather.rain));
            if (velocity.z < ball::kRestSpeed) {
                velocity.z = 0.0f;
            }
            const float tangentKeep = ball::kBounceTangentKeep + weather::kWetTangentKeepGain * weather.wetPitch;
            velocity.x *= tangentKeep;
            velocity.y *= tangentKeep;
            spin = spin * ball::kBounceSpinKeep;
        }
    }
}

float Ball::mechanicalEnergyPerKg() const
{
    return 0.5f * dot(velocity, velocity) + ball::kGravity * position.z;
}

std::uint64_t Ball::hash() const
{
    std::uint64_t h = 0;
    for (const float v : {position.x, position.y, position.z, velocity.x, velocity.y, velocity.z, spin.x, spin.y, spin.z}) {
        h = hashCombine(h, std::bit_cast<std::uint32_t>(v));
    }
    return mix64(h);
}

}

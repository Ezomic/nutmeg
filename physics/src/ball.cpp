#include "nutmeg/physics/ball.h"

#include "nutmeg/core/hash.h"

#include <bit>
#include <cmath>

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

void Ball::step()
{
    constexpr float dt = ball::kStepSeconds;
    constexpr float r = ball::kRadius;

    const bool onGround = grounded();

    if (onGround) {
        const float planar = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
        const float slow = ball::kRollDecel * dt;
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

    const float speed = length(velocity);
    Vec3 accel{0.0f, 0.0f, onGround ? 0.0f : -ball::kGravity};
    accel = accel - velocity * (ball::kDragPerMetre * speed);
    if (!onGround) {
        accel = accel + cross(spin, velocity) * ball::kMagnus;
    }

    velocity = velocity + accel * dt;
    position = position + velocity * dt;

    if (position.z < r) {
        position.z = r;
        if (velocity.z < 0.0f) {
            velocity.z = -velocity.z * ball::kRestitution;
            if (velocity.z < ball::kRestSpeed) {
                velocity.z = 0.0f;
            }
            velocity.x *= ball::kBounceTangentKeep;
            velocity.y *= ball::kBounceTangentKeep;
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

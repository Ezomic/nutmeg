#pragma once

#include "nutmeg/physics/pitch.h"
#include "nutmeg/physics/vec.h"

#include <cstdint>

namespace nutmeg::physics {

// A size 5 match ball: circumference 68 to 70 cm, mass 410 to 450 g (IFAB
// Law 2). The midpoints are used.
namespace ball {

constexpr float kStepSeconds = 1.0f / 60.0f;

constexpr float kMass = 0.43f;
constexpr float kRadius = 0.11f;
constexpr float kGravity = 9.81f;

constexpr float kAirDensity = 1.2f;
constexpr float kDragCoefficient = 0.25f;
constexpr float kCrossSection = 3.14159265f * kRadius * kRadius;
// Quadratic drag: a = -kDragPerMetre * |v| * v
constexpr float kDragPerMetre = 0.5f * kAirDensity * kDragCoefficient * kCrossSection / kMass;
// Magnus force with a lift coefficient equal to the spin parameter r*w/v:
// a = kMagnus * (w x v)
constexpr float kMagnus = 0.5f * kAirDensity * kCrossSection * kRadius / kMass;
constexpr float kSpinDecayPerSecond = 0.1f;

// Grass is far deader than the 0.85 of the FIFA lab drop test on a steel plate.
constexpr float kRestitution = 0.65f;
constexpr float kBounceTangentKeep = 0.8f;
constexpr float kBounceSpinKeep = 0.7f;
constexpr float kRestSpeed = 0.3f;

constexpr float kRollDecel = 1.2f;
constexpr float kGroundSpinDecayPerSecond = 3.0f;

}

enum class Line { None, TouchlineMinusY, TouchlinePlusY, GoalLineMinusX, GoalLinePlusX };

enum class PlayStatus { InPlay, OutOfPlay, Goal };

// scoredFor is only meaningful when status is Goal.
struct BallStatus {
    PlayStatus status = PlayStatus::InPlay;
    Line line = Line::None;
    Side scoredFor = Side::Home;

    constexpr bool operator==(const BallStatus&) const = default;
};

// The ball is out or in the goal only when all of it has crossed the line,
// so this takes the centre of the ball.
BallStatus classify(Vec3 centre);

struct Ball {
    Vec3 position{0.0f, 0.0f, ball::kRadius};
    Vec3 velocity;
    Vec3 spin; // angular velocity in rad/s, right-hand rule

    void step();
    bool grounded() const { return position.z <= ball::kRadius && velocity.z == 0.0f; }
    BallStatus status() const { return classify(position); }
    float mechanicalEnergyPerKg() const;
    std::uint64_t hash() const;
};

}

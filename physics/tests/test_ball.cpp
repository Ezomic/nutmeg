#include "nutmeg/physics/ball.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>

using namespace nutmeg::physics;

namespace {

constexpr float kR = ball::kRadius;

Ball at(Vec3 position, Vec3 velocity = {}, Vec3 spin = {})
{
    Ball b;
    b.position = position;
    b.velocity = velocity;
    b.spin = spin;
    return b;
}

void run(Ball& b, int steps)
{
    for (int i = 0; i < steps; ++i) b.step();
}

}

TEST_CASE("a dropped ball bounces lower each time and never gains energy")
{
    Ball b = at({0.0f, 0.0f, 2.0f});
    float energy = b.mechanicalEnergyPerKg();
    float peak = 2.0f;
    float previousPeak = 2.0f;
    bool rising = false;
    int bounces = 0;

    for (int i = 0; i < 60 * 12; ++i) {
        const float vzBefore = b.velocity.z;
        b.step();
        REQUIRE(b.mechanicalEnergyPerKg() <= energy);
        energy = b.mechanicalEnergyPerKg();
        REQUIRE(b.position.z >= kR);

        if (vzBefore < 0.0f && b.velocity.z >= 0.0f && b.position.z <= kR) {
            rising = true;
            peak = kR;
            ++bounces;
        } else if (rising) {
            peak = std::max(peak, b.position.z);
            if (b.velocity.z <= 0.0f) {
                REQUIRE(peak < previousPeak);
                previousPeak = peak;
                rising = false;
            }
        }
    }
    REQUIRE(bounces >= 3);
    REQUIRE(b.grounded());
}

TEST_CASE("the first bounce from two metres rebounds about e squared of the drop")
{
    Ball b = at({0.0f, 0.0f, 2.0f});
    bool landed = false;
    float peak = 0.0f;
    for (int i = 0; i < 200; ++i) {
        b.step();
        if (!landed && b.velocity.z > 0.0f) landed = true;
        if (landed) {
            peak = std::max(peak, b.position.z);
            if (b.velocity.z <= 0.0f) break;
        }
    }
    const float drop = 2.0f - kR;
    const float rebound = peak - kR;
    REQUIRE(rebound / drop == Catch::Approx(ball::kRestitution * ball::kRestitution).margin(0.05));
}

TEST_CASE("a rolling ball stops after a plausible distance")
{
    Ball b = at({0.0f, 0.0f, kR}, {10.0f, 0.0f, 0.0f});
    int steps = 0;
    while (b.velocity.x > 0.0f && steps < 60 * 60) {
        b.step();
        ++steps;
    }
    REQUIRE(b.velocity.x == 0.0f);
    REQUIRE(b.position.x > 15.0f);
    REQUIRE(b.position.x < 35.0f);
    REQUIRE(steps / 60.0f > 4.0f);
    REQUIRE(steps / 60.0f < 15.0f);
    REQUIRE(b.position.z == kR);
}

TEST_CASE("air drag shortens a kick")
{
    const Vec3 launch{25.0f * std::cos(0.5f), 0.0f, 25.0f * std::sin(0.5f)};

    auto range = [&](bool withDrag) {
        Ball b = at({0.0f, 0.0f, kR}, launch);
        if (!withDrag) {
            Vec3 p = b.position;
            Vec3 v = b.velocity;
            for (int i = 0; i < 600; ++i) {
                v.z -= ball::kGravity * ball::kStepSeconds;
                p = p + v * ball::kStepSeconds;
                if (p.z <= kR) return p.x;
            }
            return p.x;
        }
        for (int i = 0; i < 600; ++i) {
            b.step();
            if (b.velocity.z == 0.0f || (i > 5 && b.position.z <= kR)) break;
            if (b.position.z <= kR && b.velocity.z > 0.0f) break;
        }
        return b.position.x;
    };

    const float vacuum = range(false);
    const float real = range(true);
    REQUIRE(real < vacuum);
    REQUIRE(real > vacuum * 0.5f);
}

TEST_CASE("sidespin curves a kick sideways in the expected direction")
{
    Ball straight = at({-20.0f, 0.0f, kR}, {28.0f, 0.0f, 3.0f});
    Ball curled = at({-20.0f, 0.0f, kR}, {28.0f, 0.0f, 3.0f}, {0.0f, 0.0f, 60.0f});
    run(straight, 60);
    run(curled, 60);
    REQUIRE(straight.position.y == 0.0f);
    REQUIRE(curled.position.y > 1.0f);
    REQUIRE(curled.position.y < 8.0f);

    Ball other = at({-20.0f, 0.0f, kR}, {28.0f, 0.0f, 3.0f}, {0.0f, 0.0f, -60.0f});
    run(other, 60);
    REQUIRE(other.position.y == Catch::Approx(-curled.position.y).margin(1e-3));
}

TEST_CASE("crossing each line is classified")
{
    constexpr float h = pitch::kHalfLength;
    constexpr float w = pitch::kHalfWidth;

    REQUIRE(classify({0.0f, 0.0f, kR}).status == PlayStatus::InPlay);
    REQUIRE(classify({h, 0.0f, kR}).status == PlayStatus::InPlay);
    REQUIRE(classify({h + kR, 20.0f, kR}).status == PlayStatus::InPlay);
    REQUIRE(classify({0.0f, w + kR, kR}).status == PlayStatus::InPlay);

    const BallStatus plus = classify({0.0f, w + 0.5f, kR});
    REQUIRE(plus.status == PlayStatus::OutOfPlay);
    REQUIRE(plus.line == Line::TouchlinePlusY);
    const BallStatus minus = classify({10.0f, -w - 0.5f, kR});
    REQUIRE(minus.status == PlayStatus::OutOfPlay);
    REQUIRE(minus.line == Line::TouchlineMinusY);

    const BallStatus wide = classify({h + 0.5f, 10.0f, kR});
    REQUIRE(wide.status == PlayStatus::OutOfPlay);
    REQUIRE(wide.line == Line::GoalLinePlusX);
    const BallStatus wideMinus = classify({-h - 0.5f, -10.0f, kR});
    REQUIRE(wideMinus.status == PlayStatus::OutOfPlay);
    REQUIRE(wideMinus.line == Line::GoalLineMinusX);

    const BallStatus over = classify({h + 0.5f, 0.0f, pitch::kGoalHeight + 0.5f});
    REQUIRE(over.status == PlayStatus::OutOfPlay);
    REQUIRE(over.line == Line::GoalLinePlusX);

    const BallStatus postSide = classify({h + 0.5f, pitch::kHalfGoalWidth, kR});
    REQUIRE(postSide.status == PlayStatus::OutOfPlay);
}

TEST_CASE("a goal is credited to the team attacking that end")
{
    constexpr float h = pitch::kHalfLength;

    const BallStatus east = classify({h + 0.5f, 1.0f, 1.0f});
    REQUIRE(east.status == PlayStatus::Goal);
    REQUIRE(east.line == Line::GoalLinePlusX);
    REQUIRE(east.scoredFor == Side::Home);

    const BallStatus west = classify({-h - 0.5f, -1.0f, 1.0f});
    REQUIRE(west.status == PlayStatus::Goal);
    REQUIRE(west.line == Line::GoalLineMinusX);
    REQUIRE(west.scoredFor == Side::Away);
}

TEST_CASE("a shot into the corner is detected as a goal while it flies")
{
    Ball b = at({30.0f, 2.0f, kR}, {30.0f, 0.0f, 3.0f});
    PlayStatus seen = PlayStatus::InPlay;
    for (int i = 0; i < 120 && seen == PlayStatus::InPlay; ++i) {
        b.step();
        seen = b.status().status;
    }
    REQUIRE(seen == PlayStatus::Goal);
    REQUIRE(b.status().scoredFor == Side::Home);
}

TEST_CASE("pitch geometry queries")
{
    REQUIRE(pitch::inPenaltyArea({-pitch::kHalfLength + 10.0f, 15.0f}, Side::Home));
    REQUIRE_FALSE(pitch::inPenaltyArea({-pitch::kHalfLength + 10.0f, 21.0f}, Side::Home));
    REQUIRE_FALSE(pitch::inPenaltyArea({-pitch::kHalfLength + 10.0f, 0.0f}, Side::Away));
    REQUIRE(pitch::inPenaltyArea({pitch::kHalfLength - 16.0f, 0.0f}, Side::Away));
    REQUIRE(pitch::inGoalArea({-pitch::kHalfLength + 5.0f, 9.0f}, Side::Home));
    REQUIRE_FALSE(pitch::inGoalArea({-pitch::kHalfLength + 6.0f, 0.0f}, Side::Home));
    REQUIRE(pitch::inCentreCircle({9.0f, 0.0f}));
    REQUIRE_FALSE(pitch::inCentreCircle({7.0f, 7.0f}));
    REQUIRE(pitch::penaltySpot(Side::Home).x == Catch::Approx(-41.5f));
    REQUIRE(pitch::penaltySpot(Side::Away).x == Catch::Approx(41.5f));
    REQUIRE(pitch::contains({52.5f, 34.0f}));
    REQUIRE_FALSE(pitch::contains({52.6f, 0.0f}));
}

TEST_CASE("the same initial state gives the same trajectory hash")
{
    auto trajectory = [] {
        Ball b = at({-10.0f, 3.0f, kR}, {22.0f, -4.0f, 6.0f}, {5.0f, -8.0f, 40.0f});
        std::uint64_t h = 0;
        for (int i = 0; i < 600; ++i) {
            b.step();
            h = h * 1099511628211ull + b.hash();
        }
        return h;
    };
    REQUIRE(trajectory() == trajectory());

    Ball a = at({0.0f, 0.0f, kR}, {1.0f, 0.0f, 0.0f});
    Ball b = at({0.0f, 0.0f, kR}, {1.0f, 0.0f, 0.0f});
    b.velocity.x = 1.0000001f;
    run(a, 10);
    run(b, 10);
    REQUIRE(a.hash() != b.hash());
}

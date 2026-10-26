#include "nutmeg/core/dmath.h"
#include "nutmeg/gameplay/locomotion.h"
#include "nutmeg/gameplay/world.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

using namespace nutmeg;
using namespace nutmeg::gameplay;
using Catch::Approx;

namespace {

constexpr float kDt = locomotion::kStepSeconds;
constexpr float kEps = 1e-4f;

constexpr std::size_t kHome = 10;
constexpr std::size_t kAway = 21;

std::array<schema::PlayerProfile, kPlayerCount> profilesWithPace(int pace)
{
    std::array<schema::PlayerProfile, kPlayerCount> profiles;
    for (auto& p : profiles) {
        p.attributes.pace = pace;
    }
    return profiles;
}

InputFrame push(float x, float y, bool sprint)
{
    InputFrame f;
    f.moveX = x;
    f.moveY = y;
    f.buttons = sprint ? InputFrame::Sprint : 0u;
    return f;
}

float speedOf(const Player& p) { return physics::length(p.velocity); }

float wrapped(float a) { return a - std::floor(a * dmath::kInvTwoPi + 0.5f) * dmath::kTwoPi; }

}

TEST_CASE("speed and acceleration stay within the limits set by pace")
{
    for (const int pace : {1, 50, 100}) {
        World world(profilesWithPace(pace));
        PlayerInputs inputs{};
        inputs[kHome] = push(0.0f, 1.0f, true);
        float previous = 0.0f;
        float reachedTopAt = -1.0f;
        for (int tick = 0; tick < 600; ++tick) {
            world.step(inputs);
            const float speed = speedOf(world.players()[kHome]);
            REQUIRE(speed <= locomotion::sprintSpeed(pace) + kEps);
            REQUIRE(speed - previous <= locomotion::acceleration(pace) * kDt + kEps);
            if (reachedTopAt < 0.0f && speed >= locomotion::sprintSpeed(pace) - kEps) {
                reachedTopAt = static_cast<float>(tick + 1) * kDt;
            }
            previous = speed;
        }
        REQUIRE(reachedTopAt > 0.0f);
        REQUIRE(reachedTopAt == Approx(locomotion::sprintSpeed(pace) / locomotion::acceleration(pace)).margin(2 * kDt));
    }
    REQUIRE(locomotion::sprintSpeed(100) > locomotion::sprintSpeed(1));
    REQUIRE(locomotion::acceleration(100) > locomotion::acceleration(1));
}

TEST_CASE("without sprint a player tops out at the run speed")
{
    World world(profilesWithPace(60));
    PlayerInputs inputs{};
    inputs[kHome] = push(0.0f, 1.0f, false);
    for (int tick = 0; tick < 300; ++tick) {
        world.step(inputs);
    }
    REQUIRE(speedOf(world.players()[kHome]) == Approx(locomotion::runSpeed(60)).margin(kEps));
}

TEST_CASE("a diagonal stick is no faster than a straight one")
{
    World world(profilesWithPace(60));
    PlayerInputs inputs{};
    inputs[kHome] = push(1.0f, 1.0f, true);
    for (int tick = 0; tick < 150; ++tick) {
        world.step(inputs);
    }
    REQUIRE(speedOf(world.players()[kHome]) == Approx(locomotion::sprintSpeed(60)).margin(kEps));
}

TEST_CASE("facing turns no faster than the turn rate and slower at speed")
{
    REQUIRE(locomotion::turnRate(0.0f, 50) > locomotion::turnRate(5.0f, 50));
    REQUIRE(locomotion::turnRate(5.0f, 50) > locomotion::turnRate(locomotion::sprintSpeed(50), 50));

    World world(profilesWithPace(50));
    PlayerInputs inputs{};
    inputs[kHome] = push(0.0f, -1.0f, false);
    float previous = world.players()[kHome].facing;
    float total = 0.0f;
    for (int tick = 0; tick < 120; ++tick) {
        world.step(inputs);
        const Player& p = world.players()[kHome];
        const float diff = wrapped(p.facing - previous);
        REQUIRE(std::fabs(diff) <= locomotion::kTurnRateStanding * kDt + kEps);
        total += std::fabs(diff);
        previous = p.facing;
    }
    REQUIRE(std::fabs(std::fabs(world.players()[kHome].facing) - dmath::kPi) < 0.01f);
    REQUIRE(total == Approx(dmath::kPi).margin(0.01f));
}

TEST_CASE("a player at sprint turns more slowly than one standing")
{
    auto ticksToTurnAround = [](bool sprintFirst) {
        World world(profilesWithPace(50));
        PlayerInputs inputs{};
        if (sprintFirst) {
            inputs[kHome] = push(0.0f, 1.0f, true);
            for (int tick = 0; tick < 300; ++tick) {
                world.step(inputs);
            }
        }
        inputs[kHome] = push(0.0f, -1.0f, true);
        int ticks = 0;
        while (std::fabs(world.players()[kHome].facing) < dmath::kPi - 0.05f && ticks < 1000) {
            world.step(inputs);
            ++ticks;
        }
        return ticks;
    };
    REQUIRE(ticksToTurnAround(true) > ticksToTurnAround(false));
}

TEST_CASE("overlapping players are pushed apart")
{
    World world;
    world.players()[kHome].position = {10.0f, 10.0f};
    world.players()[kAway].position = {10.1f, 10.0f};
    world.step(PlayerInputs{});
    const float gap = physics::distance(world.players()[kHome].position, world.players()[kAway].position);
    REQUIRE(gap >= 2.0f * locomotion::kBodyRadius - kEps);
}

TEST_CASE("players at the same spot are separated")
{
    World world;
    world.players()[kHome].position = {10.0f, 10.0f};
    world.players()[kAway].position = {10.0f, 10.0f};
    world.step(PlayerInputs{});
    REQUIRE(physics::distance(world.players()[kHome].position, world.players()[kAway].position) >= 2.0f * locomotion::kBodyRadius - kEps);
}

TEST_CASE("a sprinter cannot run through a standing player")
{
    World world(profilesWithPace(100));
    world.players()[kHome].position = {0.0f, 0.0f};
    world.players()[kAway].position = {5.0f, 0.0f};
    PlayerInputs inputs{};
    inputs[kHome] = push(0.0f, 1.0f, true);
    for (int tick = 0; tick < 300; ++tick) {
        world.step(inputs);
        for (std::size_t i = 0; i < kPlayerCount; ++i) {
            for (std::size_t j = i + 1; j < kPlayerCount; ++j) {
                const float gap = physics::distance(world.players()[i].position, world.players()[j].position);
                REQUIRE(gap >= 2.0f * locomotion::kBodyRadius - 0.05f);
            }
        }
    }
}

TEST_CASE("players are held inside the pitch plus its margin")
{
    World world(profilesWithPace(100));
    world.players()[kHome].position = {0.0f, 20.0f};
    PlayerInputs inputs{};
    inputs[kHome] = push(0.0f, 1.0f, true);
    inputs[3] = push(-1.0f, 0.0f, true);
    for (int tick = 0; tick < 1200; ++tick) {
        world.step(inputs);
    }
    constexpr float maxX = physics::pitch::kHalfLength + locomotion::kPitchMargin;
    constexpr float maxY = physics::pitch::kHalfWidth + locomotion::kPitchMargin;
    for (const Player& p : world.players()) {
        REQUIRE(std::fabs(p.position.x) <= maxX + kEps);
        REQUIRE(std::fabs(p.position.y) <= maxY + kEps);
    }
    REQUIRE(world.players()[kHome].position.x == Approx(maxX).margin(0.5f));
    REQUIRE(world.players()[kHome].velocity.x == 0.0f);
}

TEST_CASE("the away side attacks the other way")
{
    World world;
    PlayerInputs inputs{};
    inputs[kHome] = push(0.0f, 1.0f, true);
    inputs[kAway] = push(0.0f, 1.0f, true);
    for (int tick = 0; tick < 60; ++tick) {
        world.step(inputs);
    }
    REQUIRE(world.players()[kHome].velocity.x > 0.0f);
    REQUIRE(world.players()[kAway].velocity.x < 0.0f);
}

TEST_CASE("the same inputs give the same hash and different inputs do not")
{
    auto run = [](float x) {
        World world;
        PlayerInputs inputs{};
        inputs[kHome] = push(x, 1.0f, true);
        inputs[kAway] = push(-x, 0.5f, false);
        for (int tick = 0; tick < 300; ++tick) {
            world.step(inputs);
        }
        return world.hash();
    };
    REQUIRE(run(0.3f) == run(0.3f));
    REQUIRE(run(0.3f) != run(0.4f));
    REQUIRE(World().hash() == World().hash());
}

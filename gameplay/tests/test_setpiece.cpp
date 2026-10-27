#include "nutmeg/core/dmath.h"
#include "nutmeg/gameplay/setpiece.h"
#include "nutmeg/gameplay/world.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <functional>

using namespace nutmeg;
using namespace nutmeg::gameplay;
using physics::Side;
using physics::Vec2;

namespace {

constexpr std::size_t kHomeKeeper = 0;
constexpr std::size_t kHomeBack = 2;
constexpr std::size_t kHomeWinger = 8;
constexpr std::size_t kHomeStriker = 9;
constexpr std::size_t kAwayKeeper = 11;
constexpr std::size_t kAwayStriker = 20;

using Profiles = std::array<schema::PlayerProfile, kPlayerCount>;

Profiles profiles()
{
    Profiles result;
    for (std::size_t i = 0; i < kPlayerCount; ++i) {
        result[i].id = static_cast<std::uint32_t>(i);
        result[i].position = i % kPlayersPerSide == 0 ? schema::Position::Goalkeeper : schema::Position::Midfielder;
    }
    return result;
}

void place(World& world, std::size_t index, Vec2 position, float facing)
{
    world.players()[index].position = position;
    world.players()[index].velocity = {};
    world.players()[index].facing = facing;
}

void giveBall(World& world, std::size_t index)
{
    const Player& p = world.players()[index];
    world.ball().position = {p.position.x + std::cos(p.facing) * 0.45f, p.position.y + std::sin(p.facing) * 0.45f, physics::ball::kRadius};
    world.ball().velocity = {};
    world.ball().spin = {};
    world.step(PlayerInputs{});
    REQUIRE(world.possessor() == index);
}

bool has(const World& world, EventKind kind)
{
    for (const MatchEvent& e : world.events()) {
        if (e.kind == kind) {
            return true;
        }
    }
    return false;
}

bool stepUntil(World& world, const std::function<bool(const World&)>& done, int limit = 600, const PlayerInputs& inputs = {})
{
    for (int t = 0; t < limit; ++t) {
        world.step(inputs);
        if (done(world)) {
            return true;
        }
    }
    return false;
}

// `kicker` passes the ball to `aim` hard, from where they stand, and play runs on until the restart is set.
void kickAway(World& world, std::size_t kicker, Vec2 aim)
{
    giveBall(world, kicker);
    PlayerActions actions{};
    actions[kicker].kind = ActionKind::Pass;
    actions[kicker].aim = aim;
    actions[kicker].power = 1.0f;
    world.step(PlayerInputs{}, actions);
    REQUIRE(stepUntil(world, [](const World& w) { return w.restart().has_value(); }, 400));
}

PlayerInputs holdThenRelease(std::size_t taker, std::uint32_t button, World& world, int hold = 10)
{
    PlayerInputs inputs{};
    inputs[taker].buttons = button;
    for (int t = 0; t < hold; ++t) {
        world.step(inputs);
    }
    inputs[taker].buttons = 0;
    return inputs;
}

float away(const Player& p, Vec2 spot) { return physics::distance(p.position, spot); }

Restart restartAt(RestartKind kind, Side side, Vec2 spot)
{
    Restart restart;
    restart.kind = kind;
    restart.side = side;
    restart.spot = spot;
    return restart;
}

}

TEST_CASE("a ball over the touchline becomes a throw-in for the other side")
{
    World world(profiles(), 1);
    place(world, kHomeBack, {10.0f, 30.0f}, 1.5708f);
    kickAway(world, kHomeBack, {10.0f, 45.0f});

    const Restart& restart = *world.restart();
    REQUIRE(restart.kind == RestartKind::ThrowIn);
    REQUIRE(restart.side == Side::Away);
    REQUIRE(std::abs(restart.spot.y - (physics::pitch::kHalfWidth - setpiece::kInset)) < 1e-3f);
    REQUIRE(restart.taker >= kPlayersPerSide);
    REQUIRE(world.players()[restart.taker].position.y > physics::pitch::kHalfWidth);
    REQUIRE_FALSE(world.ballDead());
}

TEST_CASE("the ball stays dead for a moment before the restart is set up")
{
    World world(profiles(), 1);
    place(world, kHomeBack, {10.0f, 30.0f}, 1.5708f);
    giveBall(world, kHomeBack);
    PlayerActions actions{};
    actions[kHomeBack].kind = ActionKind::Pass;
    actions[kHomeBack].aim = {10.0f, 45.0f};
    world.step(PlayerInputs{}, actions);
    REQUIRE(stepUntil(world, [](const World& w) { return w.ballDead(); }, 400));
    REQUIRE_FALSE(world.restart().has_value());
    REQUIRE(world.pendingRestart().has_value());
    REQUIRE(world.pendingRestart()->kind == RestartKind::ThrowIn);
}

TEST_CASE("a human takes a throw-in with the pass button: lifted, capped and aimed at a teammate")
{
    World world(profiles(), 1);
    place(world, kHomeBack, {10.0f, 30.0f}, 1.5708f);
    kickAway(world, kHomeBack, {10.0f, 45.0f});
    const std::uint32_t taker = world.restart()->taker;

    const Vec2 spot = world.restart()->spot;
    const std::size_t mate = taker == kAwayStriker ? kAwayStriker - 1 : kAwayStriker;
    place(world, mate, {spot.x + 6.0f, spot.y - 8.0f}, 0.0f);

    PlayerInputs inputs = holdThenRelease(taker, InputFrame::Pass, world);
    REQUIRE(world.restart().has_value());
    world.step(inputs);
    REQUIRE_FALSE(world.restart().has_value());
    REQUIRE(has(world, EventKind::Pass));
    REQUIRE(world.lastToucher() == taker);
    REQUIRE(world.ball().velocity.z > 0.0f);
    REQUIRE(physics::length(physics::xy(world.ball().velocity)) <= setpiece::kThrowSpeedMax + 1e-3f);
}

TEST_CASE("the opposition keeps two metres from a throw-in")
{
    World world(profiles(), 1);
    place(world, kHomeBack, {10.0f, 30.0f}, 1.5708f);
    kickAway(world, kHomeBack, {10.0f, 45.0f});
    const Vec2 spot = world.restart()->spot;
    place(world, kHomeWinger, {spot.x + 0.5f, spot.y - 0.5f}, 0.0f);
    world.step(PlayerInputs{});
    REQUIRE(away(world.players()[kHomeWinger], spot) >= setpiece::kThrowClearRadius - 0.01f);
}

TEST_CASE("a ball over the goal line off an attacker becomes a goal kick taken by the keeper")
{
    World world(profiles(), 1);
    place(world, kAwayStriker, {-45.0f, 20.0f}, dmath::kPi);
    kickAway(world, kAwayStriker, {-60.0f, 20.0f});

    const Restart& restart = *world.restart();
    REQUIRE(restart.kind == RestartKind::GoalKick);
    REQUIRE(restart.side == Side::Home);
    REQUIRE(restart.taker == kHomeKeeper);
    REQUIRE(physics::pitch::inGoalArea(restart.spot, Side::Home));
    REQUIRE(away(world.players()[kHomeKeeper], restart.spot) < 1.0f);
}

TEST_CASE("a human takes a goal kick with the shoot button")
{
    World world(profiles(), 1);
    place(world, kAwayStriker, {-45.0f, 20.0f}, dmath::kPi);
    kickAway(world, kAwayStriker, {-60.0f, 20.0f});

    PlayerInputs inputs = holdThenRelease(kHomeKeeper, InputFrame::Shoot, world);
    world.step(inputs);
    REQUIRE_FALSE(world.restart().has_value());
    REQUIRE(has(world, EventKind::Shot));
    REQUIRE(world.ball().velocity.x > 0.0f);
}

TEST_CASE("a ball over the goal line off a defender becomes a corner for the attackers")
{
    World world(profiles(), 1);
    place(world, kHomeBack, {-45.0f, 20.0f}, dmath::kPi);
    kickAway(world, kHomeBack, {-60.0f, 20.0f});

    const Restart& restart = *world.restart();
    REQUIRE(restart.kind == RestartKind::Corner);
    REQUIRE(restart.side == Side::Away);
    REQUIRE(std::abs(restart.spot.x + physics::pitch::kHalfLength) < 0.1f);
    REQUIRE(restart.spot.y > 33.0f);
    REQUIRE(restart.taker >= kPlayersPerSide);
}

TEST_CASE("the best crosser takes the corner")
{
    Profiles p = profiles();
    p[kAwayStriker].attributes.crossing = 95;
    p[kAwayStriker].attributes.passing = 95;
    World world(p, 1);
    place(world, kHomeBack, {-45.0f, 20.0f}, dmath::kPi);
    kickAway(world, kHomeBack, {-60.0f, 20.0f});
    REQUIRE(world.restart()->taker == kAwayStriker);
}

TEST_CASE("a human's pass at a corner is a cross into the air")
{
    World world(profiles(), 1);
    place(world, kHomeBack, {-45.0f, 20.0f}, dmath::kPi);
    kickAway(world, kHomeBack, {-60.0f, 20.0f});
    const std::uint32_t taker = world.restart()->taker;
    place(world, kAwayStriker == taker ? kAwayStriker - 1 : kAwayStriker, {-44.0f, 2.0f}, 0.0f);

    PlayerInputs inputs = holdThenRelease(taker, InputFrame::Pass, world);
    world.step(inputs);
    REQUIRE_FALSE(world.restart().has_value());
    REQUIRE(world.ball().velocity.z > 3.0f);
    REQUIRE(world.ball().velocity.x > 0.0f);
}

TEST_CASE("a goal is restarted with a kickoff by the side that conceded it")
{
    World world(profiles(), 1);
    place(world, kHomeStriker, {40.0f, 0.0f}, 0.0f);
    place(world, kAwayKeeper, {50.0f, 25.0f}, 0.0f);
    giveBall(world, kHomeStriker);
    PlayerActions actions{};
    actions[kHomeStriker].kind = ActionKind::Shoot;
    actions[kHomeStriker].aim = {physics::pitch::kHalfLength, 0.0f};
    actions[kHomeStriker].power = 1.0f;
    world.step(PlayerInputs{}, actions);
    bool scored = false;
    REQUIRE(stepUntil(world, [&](const World& w) {
        scored = scored || has(w, EventKind::Goal);
        return w.restart().has_value();
    }, 600));
    REQUIRE(scored);

    const Restart& restart = *world.restart();
    REQUIRE(restart.kind == RestartKind::Kickoff);
    REQUIRE(restart.side == Side::Away);
    REQUIRE(restart.spot == Vec2{0.0f, 0.0f});
    for (const Player& p : world.players()) {
        const float own = p.side == Side::Home ? -1.0f : 1.0f;
        REQUIRE(p.position.x * own >= -1e-3f);
        if (p.side == Side::Home) {
            REQUIRE(away(p, restart.spot) >= setpiece::kClearRadius - 0.01f);
        }
    }
}

TEST_CASE("a human takes a kickoff with the pass button")
{
    World world(profiles(), 1);
    world.kickoff(Side::Home);
    const std::uint32_t taker = world.restart()->taker;
    REQUIRE(taker < kPlayersPerSide);
    place(world, taker == kHomeStriker ? kHomeStriker - 1 : kHomeStriker, {-8.0f, 5.0f}, 0.0f);

    PlayerInputs inputs = holdThenRelease(taker, InputFrame::Pass, world);
    world.step(inputs);
    REQUIRE_FALSE(world.restart().has_value());
    REQUIRE(has(world, EventKind::Pass));
    REQUIRE(world.lastToucher() == taker);
}

TEST_CASE("the other side cannot touch the ball while a restart waits")
{
    World world(profiles(), 1);
    world.kickoff(Side::Home);
    const Vec2 spot = world.restart()->spot;
    place(world, kAwayStriker, {spot.x + 9.2f, spot.y}, dmath::kPi);
    PlayerActions actions{};
    actions[kAwayStriker].kind = ActionKind::Shoot;
    actions[kAwayStriker].aim = {-52.5f, 0.0f};
    for (int t = 0; t < 30; ++t) {
        world.step(PlayerInputs{}, actions);
    }
    REQUIRE(world.restart().has_value());
    REQUIRE(world.ball().position.x == 0.0f);
}

TEST_CASE("a direct free kick keeps the wall back and can score")
{
    World world(profiles(), 1);
    const Vec2 spot{30.0f, 3.0f};
    place(world, kHomeBack, {spot.x + 3.0f, spot.y}, 0.0f);
    world.restartPlay(restartAt(RestartKind::DirectFreeKick, Side::Home, spot));
    world.step(PlayerInputs{});
    const std::uint32_t taker = world.restart()->taker;
    REQUIRE(taker < kPlayersPerSide);
    for (const Player& p : world.players()) {
        if (p.side == Side::Away) {
            REQUIRE(away(p, spot) >= setpiece::kClearRadius - 0.01f);
        }
    }

    PlayerInputs inputs = holdThenRelease(taker, InputFrame::Shoot, world);
    world.step(inputs);
    REQUIRE_FALSE(world.restart().has_value());
    REQUIRE(has(world, EventKind::Shot));
    REQUIRE(world.ball().velocity.x > 0.0f);
}

TEST_CASE("a goal straight from an indirect free kick does not count")
{
    World world(profiles(), 1);
    const Vec2 spot{44.0f, 0.0f};
    for (std::size_t i = kAwayKeeper; i < kPlayerCount; ++i) {
        place(world, i, {50.0f, ((i % 2) != 0 ? 1.0f : -1.0f) * (10.0f + 2.0f * static_cast<float>(i - kAwayKeeper))}, 0.0f);
    }
    world.restartPlay(restartAt(RestartKind::IndirectFreeKick, Side::Home, spot));
    const std::uint32_t taker = world.restart()->taker;

    PlayerActions actions{};
    actions[taker].kind = ActionKind::Shoot;
    actions[taker].aim = {physics::pitch::kHalfLength, 0.0f};
    actions[taker].power = 1.0f;
    world.step(PlayerInputs{}, actions);
    bool goal = false;
    bool out = false;
    stepUntil(world, [&](const World& w) {
        goal = goal || has(w, EventKind::Goal);
        out = out || has(w, EventKind::OutOfPlay);
        return out || goal;
    }, 300);
    REQUIRE_FALSE(goal);
    REQUIRE(out);
    REQUIRE(world.pendingRestart()->kind == RestartKind::GoalKick);
    REQUIRE(world.pendingRestart()->side == Side::Away);
}

TEST_CASE("a penalty puts everyone but the taker and keeper outside the area and arc")
{
    World world(profiles(), 1);
    const Vec2 spot = physics::pitch::penaltySpot(Side::Away);
    for (std::size_t i = 0; i < kPlayerCount; ++i) {
        place(world, i, {spot.x - 1.0f, -10.0f + static_cast<float>(i)}, 0.0f);
    }
    world.restartPlay(restartAt(RestartKind::Penalty, Side::Home, spot));
    world.step(PlayerInputs{});

    const std::uint32_t taker = world.restart()->taker;
    REQUIRE(taker < kPlayersPerSide);
    REQUIRE(away(world.players()[taker], spot) < 1.0f);
    REQUIRE(world.players()[kAwayKeeper].position.x > physics::pitch::kHalfLength - 1.0f);
    for (const Player& p : world.players()) {
        if (p.id == taker || p.id == kAwayKeeper) {
            continue;
        }
        REQUIRE_FALSE(physics::pitch::inPenaltyArea(p.position, Side::Away));
        REQUIRE(away(p, spot) >= setpiece::kClearRadius - 0.01f);
    }
}

TEST_CASE("a human takes a penalty with the shoot button")
{
    World world(profiles(), 1);
    const Vec2 spot = physics::pitch::penaltySpot(Side::Away);
    world.restartPlay(restartAt(RestartKind::Penalty, Side::Home, spot));
    const std::uint32_t taker = world.restart()->taker;

    PlayerInputs inputs = holdThenRelease(taker, InputFrame::Shoot, world);
    world.step(inputs);
    REQUIRE_FALSE(world.restart().has_value());
    REQUIRE(has(world, EventKind::Shot));
    REQUIRE(world.ball().velocity.x > 10.0f);
}

TEST_CASE("a throw-in is not offside")
{
    World world(profiles(), 1);
    for (std::size_t i = 1; i < kPlayersPerSide; ++i) {
        place(world, i, {-25.0f, -30.0f + 6.0f * static_cast<float>(i)}, 0.0f);
    }
    place(world, kHomeKeeper, {-50.0f, 0.0f}, 0.0f);
    const std::size_t runner = kAwayStriker;
    place(world, runner, {-40.0f, 20.0f}, 0.0f);
    place(world, 15, {-29.0f, 33.0f}, 0.0f);
    REQUIRE(inOffsidePosition(world.players().data(), kPlayerCount, world.players()[runner], -30.0f));

    world.restartPlay(restartAt(RestartKind::ThrowIn, Side::Away, {-30.0f, physics::pitch::kHalfWidth - setpiece::kInset}));
    const std::uint32_t taker = world.restart()->taker;
    REQUIRE(taker != runner);

    PlayerActions actions{};
    actions[taker].kind = ActionKind::Pass;
    actions[taker].target = static_cast<std::uint32_t>(runner);
    actions[taker].power = 0.8f;
    world.step(PlayerInputs{}, actions);
    bool offside = false;
    bool received = false;
    stepUntil(world, [&](const World& w) {
        offside = offside || has(w, EventKind::Offside);
        received = received || w.possessor() == runner;
        return offside || received;
    }, 300);
    REQUIRE_FALSE(offside);
}

TEST_CASE("the same restart sequence gives the same hash")
{
    const auto run = [] {
        World world(profiles(), 7);
        place(world, kHomeBack, {10.0f, 30.0f}, 1.5708f);
        kickAway(world, kHomeBack, {10.0f, 45.0f});
        PlayerInputs inputs = holdThenRelease(world.restart()->taker, InputFrame::Pass, world);
        for (int t = 0; t < 120; ++t) {
            world.step(inputs);
        }
        return world.hash();
    };
    REQUIRE(run() == run());
}

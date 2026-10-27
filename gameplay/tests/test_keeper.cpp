#include "nutmeg/core/rng.h"
#include "nutmeg/gameplay/keeper.h"
#include "nutmeg/gameplay/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>

using namespace nutmeg;
using namespace nutmeg::gameplay;
using physics::Side;
using physics::Vec2;

namespace {

constexpr std::size_t kHomeKeeper = 0;
constexpr std::size_t kShooter = 21;
constexpr std::size_t kCrosser = 20;

// The band for the share of on-target shots the keeper saves. Shots on target
// are 7.5 to 11 a match and goals 2.4 to 3.2 in data/realism_bands.json, which
// leaves 65 to 78 percent saved.
constexpr double kSaveBandLow = 0.65;
constexpr double kSaveBandHigh = 0.78;
constexpr int kBatch = 600;

using Profiles = std::array<schema::PlayerProfile, kPlayerCount>;

Profiles profiles(int keeperRating = schema::defaultRating)
{
    Profiles result;
    for (std::size_t i = 0; i < kPlayerCount; ++i) {
        result[i].id = static_cast<std::uint32_t>(i);
        if (i % kPlayersPerSide == 0) {
            result[i].position = schema::Position::Goalkeeper;
            auto& g = result[i].goalkeeper;
            g.reflexes = g.handling = g.positioning = g.diving = g.kicking = keeperRating;
        }
    }
    return result;
}

void parkOthers(World& world, std::initializer_list<std::size_t> keep)
{
    int n = 0;
    for (std::size_t i = 0; i < kPlayerCount; ++i) {
        if (std::find(keep.begin(), keep.end(), i) != keep.end()) {
            continue;
        }
        world.players()[i].position = {-45.0f + 4.0f * static_cast<float>(n++), i < kPlayersPerSide ? -33.0f : 33.0f};
        world.players()[i].velocity = {};
    }
}

void place(World& world, std::size_t index, Vec2 position, float facing)
{
    world.players()[index].position = position;
    world.players()[index].velocity = {};
    world.players()[index].facing = facing;
}

void run(World& world, int ticks)
{
    for (int i = 0; i < ticks; ++i) {
        world.step(PlayerInputs{});
    }
}

bool has(const World& world, EventKind kind)
{
    return std::any_of(world.events().begin(), world.events().end(), [kind](const MatchEvent& e) { return e.kind == kind; });
}

struct Outcome {
    bool goal = false;
    bool saved = false;
};

// One shot at the Home goal from a spot chosen by `rng`, aimed inside the posts.
Outcome shoot(const Profiles& profs, std::uint64_t seed, Rng& rng)
{
    World world(profs, seed);
    parkOthers(world, {kHomeKeeper, kShooter});
    const float x = -52.5f + 8.0f + rng.nextFloat() * 20.0f;
    const float y = (rng.nextFloat() * 2.0f - 1.0f) * 16.0f;
    place(world, kShooter, {x, y}, 3.14159265f);
    const Vec2 forward{-1.0f, 0.0f};
    world.ball().position = {x + forward.x * 0.45f, y, physics::ball::kRadius};
    world.ball().velocity = {};
    run(world, 90);

    ActionRequest shot;
    shot.kind = ActionKind::Shoot;
    shot.aim = {-52.5f, (rng.nextFloat() + rng.nextFloat() - 1.0f) * physics::pitch::kHalfGoalWidth * 0.9f};
    shot.power = 0.6f + rng.nextFloat() * 0.4f;
    PlayerActions actions;
    actions[kShooter] = shot;
    world.step(PlayerInputs{}, actions);

    Outcome out;
    for (int tick = 0; tick < 240 && !out.goal && !out.saved && !world.ballDead(); ++tick) {
        world.step(PlayerInputs{});
        for (const MatchEvent& e : world.events()) {
            out.goal = out.goal || e.kind == EventKind::Goal;
            out.saved = out.saved || e.kind == EventKind::Save;
        }
    }
    return out;
}

double saveRate(const Profiles& profs, std::uint64_t seed)
{
    Rng rng = Rng(seed).derive("shots");
    int goals = 0;
    int saves = 0;
    for (int i = 0; i < kBatch; ++i) {
        const Outcome o = shoot(profs, seed * 100000 + static_cast<std::uint64_t>(i), rng);
        goals += o.goal ? 1 : 0;
        saves += o.saved ? 1 : 0;
    }
    return static_cast<double>(saves) / static_cast<double>(goals + saves);
}

}

TEST_CASE("the guard point sits on the line from goal to ball, deeper as the ball moves away")
{
    const Vec2 near = keeperGuardPoint(Side::Home, {-40.0f, 0.0f}, {}, 50);
    const Vec2 far = keeperGuardPoint(Side::Home, {-10.0f, 0.0f}, {}, 50);
    CHECK(near.x > -52.5f);
    CHECK(far.x > near.x);
    CHECK(std::abs(far.y) < 1e-4f);

    const Vec2 wide = keeperGuardPoint(Side::Home, {-45.0f, 20.0f}, {}, 50);
    CHECK(wide.y > 0.0f);
    CHECK(wide.x > -52.5f);

    const Vec2 away = keeperGuardPoint(Side::Away, {10.0f, 0.0f}, {}, 50);
    CHECK(away.x < 52.5f);
    CHECK(away.x > 50.0f - 6.5f);
}

TEST_CASE("a keeper takes up the guard point while the ball is elsewhere")
{
    World world(profiles(), 3);
    world.ball().position = {-20.0f, 12.0f, physics::ball::kRadius};
    world.ball().velocity = {};
    place(world, kHomeKeeper, {-50.0f, -6.0f}, 0.0f);
    parkOthers(world, {kHomeKeeper});
    run(world, 300);

    const Vec2 want = keeperGuardPoint(Side::Home, {-20.0f, 12.0f}, {}, 50);
    CHECK(physics::distance(world.players()[kHomeKeeper].position, want) < 0.6f);
}

TEST_CASE("a keeper played by a person is not driven")
{
    World world(profiles(), 3);
    parkOthers(world, {kHomeKeeper});
    place(world, kHomeKeeper, {-50.0f, 0.0f}, 0.0f);
    world.ball().position = {-10.0f, 10.0f, physics::ball::kRadius};
    PlayerInputs inputs{};
    inputs[kHomeKeeper].moveX = 1.0f;
    for (int i = 0; i < 60; ++i) {
        world.step(inputs);
    }
    CHECK(std::abs(world.players()[kHomeKeeper].position.y) > 2.0f);
}

TEST_CASE("the save rate over a batch of shots sits inside the realism band")
{
    const double rate = saveRate(profiles(), 7);
    INFO("save rate " << rate);
    CHECK(rate >= kSaveBandLow);
    CHECK(rate <= kSaveBandHigh);
}

TEST_CASE("a better keeper saves more than a worse one")
{
    const double good = saveRate(profiles(85), 11);
    const double poor = saveRate(profiles(20), 11);
    INFO("good " << good << " poor " << poor);
    CHECK(good > poor + 0.1);
}

TEST_CASE("a saved shot is announced and the keeper either holds it or parries it away from goal")
{
    int held = 0;
    int parried = 0;
    Rng rng = Rng(5).derive("shots");
    for (int i = 0; i < 80; ++i) {
        World world(profiles(), 500 + static_cast<std::uint64_t>(i));
        parkOthers(world, {kHomeKeeper, kShooter});
        place(world, kShooter, {-35.0f, 0.0f}, 3.14159265f);
        world.ball().position = {-35.45f, 0.0f, physics::ball::kRadius};
        run(world, 90);
        ActionRequest shot;
        shot.kind = ActionKind::Shoot;
        shot.aim = {-52.5f, (rng.nextFloat() * 2.0f - 1.0f) * 2.5f};
        shot.power = 0.7f;
        PlayerActions actions;
        actions[kShooter] = shot;
        world.step(PlayerInputs{}, actions);
        for (int t = 0; t < 120; ++t) {
            world.step(PlayerInputs{});
            for (const MatchEvent& e : world.events()) {
                if (e.kind != EventKind::Save) {
                    continue;
                }
                CHECK(e.player == kHomeKeeper);
                CHECK(e.other == kShooter);
                if (e.value > 0.5f) {
                    ++held;
                    CHECK(world.possessor() == kHomeKeeper);
                } else {
                    ++parried;
                    CHECK(world.ball().velocity.x > 0.0f);
                }
            }
        }
    }
    CHECK(held > 0);
    CHECK(parried > 0);
}

TEST_CASE("a keeper claims a cross into the six-yard box when nobody else is near")
{
    int claimed = 0;
    for (int i = 0; i < 30; ++i) {
        World world(profiles(80), 900 + static_cast<std::uint64_t>(i));
        parkOthers(world, {kHomeKeeper, kCrosser});
        place(world, kCrosser, {-30.0f, 28.0f}, 3.14159265f);
        world.ball().position = {-30.45f, 28.0f, physics::ball::kRadius};
        run(world, 60);
        ActionRequest cross;
        cross.kind = ActionKind::Cross;
        cross.aim = {-48.0f, 3.0f};
        PlayerActions actions;
        actions[kCrosser] = cross;
        world.step(PlayerInputs{}, actions);
        bool held = false;
        for (int t = 0; t < 180 && !held; ++t) {
            world.step(PlayerInputs{});
            for (const MatchEvent& e : world.events()) {
                held = held || (e.kind == EventKind::Claim && e.player == kHomeKeeper && e.value > 0.5f && world.possessor() == kHomeKeeper);
            }
        }
        claimed += held ? 1 : 0;
    }
    CHECK(claimed >= 20);
}

TEST_CASE("a keeper with the ball waits, then kicks it to a teammate")
{
    World world(profiles(70), 21);
    parkOthers(world, {kHomeKeeper, 8});
    place(world, kHomeKeeper, {-48.0f, 0.0f}, 0.0f);
    place(world, 8, {-20.0f, 10.0f}, 0.0f);
    world.ball().position = {-47.55f, 0.0f, physics::ball::kRadius};
    world.ball().velocity = {};
    world.step(PlayerInputs{});
    REQUIRE(world.possessor() == kHomeKeeper);

    int kickedAt = -1;
    for (int t = 0; t < 200 && kickedAt < 0; ++t) {
        world.step(PlayerInputs{});
        if (has(world, EventKind::Pass)) {
            kickedAt = t;
        }
    }
    CHECK(kickedAt >= keeper::kHoldTicks - 5);
    CHECK(world.possessor() != kHomeKeeper);
    CHECK(world.ball().velocity.x > 0.0f);
}

TEST_CASE("a keeper holding the ball cannot be tackled in his own area")
{
    World world(profiles(), 4);
    parkOthers(world, {kHomeKeeper, 15});
    place(world, kHomeKeeper, {-48.0f, 0.0f}, 0.0f);
    place(world, 15, {-47.0f, 0.0f}, 3.14159265f);
    world.ball().position = {-47.55f, 0.0f, physics::ball::kRadius};
    world.step(PlayerInputs{});
    REQUIRE(world.possessor() == kHomeKeeper);

    ActionRequest tackle;
    tackle.kind = ActionKind::Tackle;
    PlayerActions actions;
    actions[15] = tackle;
    world.step(PlayerInputs{}, actions);
    CHECK_FALSE(has(world, EventKind::Tackle));
    CHECK(world.possessor() == kHomeKeeper);
}

TEST_CASE("the same seed gives the same keeper play")
{
    const auto play = [] {
        World world(profiles(), 13);
        parkOthers(world, {kHomeKeeper, kShooter});
        place(world, kShooter, {-35.0f, 4.0f}, 3.14159265f);
        world.ball().position = {-35.45f, 4.0f, physics::ball::kRadius};
        run(world, 90);
        ActionRequest shot;
        shot.kind = ActionKind::Shoot;
        shot.aim = {-52.5f, -2.0f};
        PlayerActions actions;
        actions[kShooter] = shot;
        world.step(PlayerInputs{}, actions);
        run(world, 120);
        return world.hash();
    };
    CHECK(play() == play());
}

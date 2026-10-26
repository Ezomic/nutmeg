#include "nutmeg/core/dmath.h"
#include "nutmeg/core/rng.h"
#include "nutmeg/gameplay/actions.h"
#include "nutmeg/gameplay/world.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

using namespace nutmeg;
using namespace nutmeg::gameplay;
using physics::Vec2;
using Catch::Approx;

namespace {

constexpr std::size_t kKicker = 10;
constexpr std::size_t kMate = 9;
constexpr std::size_t kCarrier = 11;
constexpr std::size_t kAwayMate = 12;

using Profiles = std::array<schema::PlayerProfile, kPlayerCount>;

Profiles profiles()
{
    Profiles result;
    for (std::size_t i = 0; i < kPlayerCount; ++i) {
        result[i].id = static_cast<std::uint32_t>(i);
    }
    return result;
}

void parkOthers(World& world, std::initializer_list<std::size_t> keep)
{
    int n = 0;
    for (std::size_t i = 0; i < kPlayerCount; ++i) {
        bool kept = false;
        for (const std::size_t k : keep) {
            kept = kept || k == i;
        }
        if (kept) {
            continue;
        }
        world.players()[i].position = {-50.0f + 4.0f * static_cast<float>(n++), i < kPlayersPerSide ? -33.0f : 33.0f};
    }
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
    const Vec2 forward{std::cos(p.facing), std::sin(p.facing)};
    world.ball().position = {p.position.x + forward.x * 0.45f, p.position.y + forward.y * 0.45f, physics::ball::kRadius};
    world.ball().velocity = {};
    world.ball().spin = {};
    world.step(PlayerInputs{});
    REQUIRE(world.possessor() == index);
}

void run(World& world, int ticks)
{
    for (int i = 0; i < ticks; ++i) {
        world.step(PlayerInputs{});
    }
}

ActionRequest request(ActionKind kind, Vec2 aim, float power, std::uint32_t target = kNoPlayer)
{
    ActionRequest r;
    r.kind = kind;
    r.aim = aim;
    r.power = power;
    r.target = target;
    return r;
}

const MatchEvent* find(const World& world, EventKind kind)
{
    for (const MatchEvent& e : world.events()) {
        if (e.kind == kind) {
            return &e;
        }
    }
    return nullptr;
}

float planarSpeed(const physics::Ball& b) { return std::sqrt(b.velocity.x * b.velocity.x + b.velocity.y * b.velocity.y); }

}

TEST_CASE("a player takes a loose ball at rest and keeps it while dribbling")
{
    World world(profiles(), 1);
    parkOthers(world, {kKicker});
    place(world, kKicker, {-10.0f, 0.0f}, 0.0f);
    world.ball().position = {-9.5f, 0.0f, physics::ball::kRadius};

    PlayerInputs inputs{};
    inputs[kKicker].moveY = 1.0f;
    inputs[kKicker].buttons = InputFrame::Sprint;

    world.step(inputs);
    REQUIRE(world.possessor() == kKicker);
    const MatchEvent* touch = find(world, EventKind::Touch);
    REQUIRE(touch != nullptr);
    REQUIRE(touch->value == Approx(1.0f).margin(0.2f));

    for (int tick = 0; tick < 300; ++tick) {
        world.step(inputs);
        REQUIRE(world.possessor() == kKicker);
        const Player& p = world.players()[kKicker];
        REQUIRE(physics::distance(p.position, physics::xy(world.ball().position)) <= 1.0f);
    }
    REQUIRE(world.players()[kKicker].position.x > 20.0f);
}

TEST_CASE("a dribbler with better dribbling keeps the ball closer at speed")
{
    float gap[2];
    int i = 0;
    for (const int dribbling : {10, 90}) {
        Profiles ps = profiles();
        ps[kKicker].attributes.dribbling = dribbling;
        World world(ps, 1);
        parkOthers(world, {kKicker});
        place(world, kKicker, {-10.0f, 0.0f}, 0.0f);
        giveBall(world, kKicker);
        PlayerInputs inputs{};
        inputs[kKicker].moveY = 1.0f;
        inputs[kKicker].buttons = InputFrame::Sprint;
        run(world, 1);
        for (int t = 0; t < 240; ++t) {
            world.step(inputs);
        }
        gap[i++] = physics::distance(world.players()[kKicker].position, physics::xy(world.ball().position));
    }
    REQUIRE(gap[1] < gap[0]);
}

TEST_CASE("a ball that is too fast for the first touch runs away, a slow one is trapped")
{
    const auto touchValue = [](int firstTouch, float ballSpeed, bool& controlled, float& after) {
        Profiles ps = profiles();
        ps[kKicker].attributes.firstTouch = firstTouch;
        World world(ps, 1);
        parkOthers(world, {kKicker});
        place(world, kKicker, {0.0f, 0.0f}, 0.0f);
        world.ball().position = {6.0f, 0.0f, physics::ball::kRadius};
        world.ball().velocity = {-ballSpeed, 0.0f, 0.0f};
        for (int t = 0; t < 400; ++t) {
            world.step(PlayerInputs{});
            if (const MatchEvent* e = find(world, EventKind::Touch)) {
                controlled = world.possessor() == kKicker;
                after = planarSpeed(world.ball());
                return e->value;
            }
        }
        FAIL("the ball never reached the player");
        return 0.0f;
    };

    bool controlled = false;
    float after = 0.0f;
    const float skilled = touchValue(90, 9.0f, controlled, after);
    REQUIRE(controlled);
    const float poor = touchValue(10, 9.0f, controlled, after);
    REQUIRE_FALSE(controlled);
    REQUIRE(poor == 0.0f);
    REQUIRE(skilled > poor);

    touchValue(50, 20.0f, controlled, after);
    REQUIRE_FALSE(controlled);
    REQUIRE(after < 20.0f * 0.6f);
}

TEST_CASE("a loose ball is won more often by the better first touch")
{
    int wins[2] = {0, 0};
    for (std::uint64_t seed = 0; seed < 400; ++seed) {
        Profiles ps = profiles();
        ps[kKicker].attributes.firstTouch = 90;
        ps[kCarrier].attributes.firstTouch = 10;
        World world(ps, seed);
        parkOthers(world, {kKicker, kCarrier});
        place(world, kKicker, {-0.5f, 0.0f}, 0.0f);
        place(world, kCarrier, {0.5f, 0.0f}, dmath::kPi);
        world.ball().position = {0.0f, 0.0f, physics::ball::kRadius};
        world.step(PlayerInputs{});
        REQUIRE(world.possessor() != kNoPlayer);
        ++wins[world.possessor() == kKicker ? 0 : 1];
    }
    REQUIRE(wins[0] > wins[1] * 2);
}

TEST_CASE("a ground pass travels about the distance its power implies")
{
    for (const float power : {0.2f, 0.4f, 0.55f, 0.9f}) {
        Profiles ps = profiles();
        ps[kKicker].attributes.passing = 100;
        World world(ps, 3);
        parkOthers(world, {kKicker});
        place(world, kKicker, {-45.0f, 0.0f}, 0.0f);
        giveBall(world, kKicker);
        const Vec2 start = physics::xy(world.ball().position);

        PlayerActions actions{};
        actions[kKicker] = request(ActionKind::Pass, {0.0f, 0.0f}, power);
        world.step(PlayerInputs{}, actions);
        const MatchEvent* pass = find(world, EventKind::Pass);
        REQUIRE(pass != nullptr);
        REQUIRE(pass->player == kKicker);
        REQUIRE(pass->value == Approx(passSpeed(power)).epsilon(0.05));
        REQUIRE(world.possessor() == kNoPlayer);

        for (int t = 0; t < 900 && (world.ball().velocity.x != 0.0f || world.ball().velocity.y != 0.0f); ++t) {
            world.step(PlayerInputs{});
        }
        const float travelled = physics::distance(start, physics::xy(world.ball().position));
        REQUIRE(travelled == Approx(rollDistance(passSpeed(power))).epsilon(0.1));
    }
}

TEST_CASE("a pass to a stationary teammate arrives where he can control it")
{
    for (std::uint64_t seed = 0; seed < 20; ++seed) {
        Profiles ps = profiles();
        ps[kKicker].attributes.passing = 100;
        World world(ps, seed);
        parkOthers(world, {kKicker, kMate});
        place(world, kKicker, {-30.0f, 0.0f}, 0.0f);
        place(world, kMate, {-10.0f, 4.0f}, 0.0f);
        giveBall(world, kKicker);

        const float distance = physics::distance(world.players()[kKicker].position, world.players()[kMate].position);
        PlayerActions actions{};
        actions[kKicker] = request(ActionKind::Pass, {}, passPowerForDistance(distance), kMate);
        world.step(PlayerInputs{}, actions);
        for (int t = 0; t < 300 && world.possessor() == kNoPlayer; ++t) {
            world.step(PlayerInputs{});
        }
        REQUIRE(world.possessor() == kMate);
    }
}

TEST_CASE("a through ball leads a running teammate")
{
    int received = 0;
    for (std::uint64_t seed = 0; seed < 20; ++seed) {
        Profiles ps = profiles();
        ps[kKicker].attributes.passing = 100;
        World world(ps, seed);
        parkOthers(world, {kKicker, kMate});
        place(world, kKicker, {-30.0f, 0.0f}, 0.0f);
        place(world, kMate, {-22.0f, 7.0f}, 0.0f);
        world.players()[kMate].velocity = {locomotion::runSpeed(50), 0.0f};
        giveBall(world, kKicker);

        PlayerInputs inputs{};
        inputs[kMate].moveY = 1.0f;
        PlayerActions actions{};
        actions[kKicker] = request(ActionKind::Through, {}, 0.6f, kMate);
        world.step(inputs, actions);
        for (int t = 0; t < 300 && world.possessor() == kNoPlayer; ++t) {
            world.step(inputs);
        }
        received += world.possessor() == kMate ? 1 : 0;
    }
    REQUIRE(received >= 18);
}

TEST_CASE("a cross lands near its target")
{
    Profiles ps = profiles();
    ps[kKicker].attributes.crossing = 100;
    World world(ps, 5);
    parkOthers(world, {kKicker});
    place(world, kKicker, {30.0f, 25.0f}, dmath::kPi * 0.5f);
    giveBall(world, kKicker);

    const Vec2 target{44.0f, 2.0f};
    PlayerActions actions{};
    actions[kKicker] = request(ActionKind::Cross, target, 1.0f);
    world.step(PlayerInputs{}, actions);
    REQUIRE(world.ball().velocity.z > 0.0f);

    float previousVz = world.ball().velocity.z;
    for (int t = 0; t < 400; ++t) {
        world.step(PlayerInputs{});
        const bool bounced = previousVz < 0.0f && world.ball().velocity.z > 0.0f;
        previousVz = world.ball().velocity.z;
        if (bounced) {
            break;
        }
    }
    REQUIRE(physics::distance(physics::xy(world.ball().position), target) < 6.0f);
}

namespace {

struct ShotStats {
    float meanLateralMiss = 0.0f;
    float onTarget = 0.0f;
};

ShotStats shoot(int finishing, int composure, float pressure, float power)
{
    constexpr int kShots = 2000;
    Player kicker;
    kicker.position = {22.0f, 0.0f};
    kicker.profile.attributes.finishing = finishing;
    kicker.profile.attributes.composure = composure;
    const Vec2 aim{physics::pitch::kHalfLength, 0.0f};

    ShotStats stats;
    for (int seed = 0; seed < kShots; ++seed) {
        Rng rng(static_cast<std::uint64_t>(seed));
        const Kick k = shotKick(kicker, aim, power, pressure, false, rng);
        const float t = (aim.x - kicker.position.x) / k.velocity.x;
        const float y = k.velocity.y * t;
        const float z = physics::ball::kRadius + k.velocity.z * t - 0.5f * physics::ball::kGravity * t * t;
        stats.meanLateralMiss += std::abs(y);
        stats.onTarget += (std::abs(y) <= physics::pitch::kHalfGoalWidth && z <= physics::pitch::kGoalHeight) ? 1.0f : 0.0f;
    }
    stats.meanLateralMiss /= kShots;
    stats.onTarget /= kShots;
    return stats;
}

}

TEST_CASE("shot accuracy degrades with lower finishing")
{
    const ShotStats poor = shoot(10, 10, 0.0f, 0.9f);
    const ShotStats average = shoot(50, 50, 0.0f, 0.9f);
    const ShotStats clinical = shoot(95, 95, 0.0f, 0.9f);
    REQUIRE(clinical.meanLateralMiss < average.meanLateralMiss);
    REQUIRE(average.meanLateralMiss < poor.meanLateralMiss);
    REQUIRE(poor.meanLateralMiss > clinical.meanLateralMiss * 3.0f);
    REQUIRE(clinical.onTarget >= poor.onTarget);
}

TEST_CASE("pressure hurts shot accuracy, and composure shields against it")
{
    const ShotStats calm = shoot(70, 70, 0.0f, 0.9f);
    const ShotStats pressedComposed = shoot(70, 95, 1.0f, 0.9f);
    const ShotStats pressedRattled = shoot(70, 5, 1.0f, 0.9f);
    REQUIRE(pressedRattled.meanLateralMiss > calm.meanLateralMiss);
    REQUIRE(pressedRattled.meanLateralMiss > pressedComposed.meanLateralMiss);
}

TEST_CASE("a shot leaves with a speed set by its power, and a lofted one rises")
{
    Player kicker;
    kicker.position = {22.0f, 0.0f};
    kicker.profile.attributes.finishing = 100;
    kicker.profile.attributes.composure = 100;
    Rng rng(1);
    const Vec2 aim{physics::pitch::kHalfLength, 0.0f};
    const Kick weak = shotKick(kicker, aim, 0.2f, 0.0f, false, rng);
    const Kick hard = shotKick(kicker, aim, 1.0f, 0.0f, false, rng);
    REQUIRE(physics::length(hard.velocity) > physics::length(weak.velocity) * 1.8f);

    const Kick chip = shotKick(kicker, aim, 0.6f, 0.0f, true, rng);
    const Kick drive = shotKick(kicker, aim, 0.6f, 0.0f, false, rng);
    REQUIRE(chip.velocity.z > drive.velocity.z * 3.0f);
}

TEST_CASE("pass error shrinks with passing and grows under pressure")
{
    const auto meanError = [](int passing, float pressure) {
        Player kicker;
        kicker.profile.attributes.passing = passing;
        float total = 0.0f;
        for (int seed = 0; seed < 1000; ++seed) {
            Rng rng(static_cast<std::uint64_t>(seed));
            const Kick k = passKick(kicker, {20.0f, 0.0f}, 0.6f, pressure, false, rng);
            total += std::abs(k.velocity.y) / k.velocity.x;
        }
        return total / 1000.0f;
    };
    REQUIRE(meanError(95, 0.0f) < meanError(50, 0.0f));
    REQUIRE(meanError(50, 0.0f) < meanError(5, 0.0f));
    REQUIRE(meanError(50, 1.0f) > meanError(50, 0.0f) * 1.5f);
}

TEST_CASE("tackle success chance scales with the attributes of both players")
{
    Player tackler;
    Player carrier;
    const float even = tackleSuccessChance(tackler, carrier);
    tackler.profile.attributes.tackling = 90;
    REQUIRE(tackleSuccessChance(tackler, carrier) > even);
    tackler.profile.attributes.tackling = 10;
    REQUIRE(tackleSuccessChance(tackler, carrier) < even);

    tackler.profile.attributes.tackling = 50;
    carrier.profile.attributes.dribbling = 90;
    REQUIRE(tackleSuccessChance(tackler, carrier) < even);
    carrier.profile.attributes.dribbling = 50;
    carrier.profile.attributes.strength = 90;
    REQUIRE(tackleSuccessChance(tackler, carrier) < even);
    tackler.profile.attributes.strength = 90;
    REQUIRE(tackleSuccessChance(tackler, carrier) == Approx(even));
}

namespace {

struct TackleStats {
    int won = 0;
    int fouls = 0;
};

TackleStats tackleMany(int tackling, int dribbling, int aggression)
{
    TackleStats stats;
    for (std::uint64_t seed = 0; seed < 600; ++seed) {
        Profiles ps = profiles();
        ps[kKicker].attributes.tackling = tackling;
        ps[kKicker].attributes.aggression = aggression;
        ps[kCarrier].attributes.dribbling = dribbling;
        World world(ps, seed);
        parkOthers(world, {kKicker, kCarrier});
        place(world, kCarrier, {0.0f, 0.0f}, 0.0f);
        place(world, kKicker, {-1.0f, 0.0f}, 0.0f);
        giveBall(world, kCarrier);

        PlayerActions actions{};
        actions[kKicker].kind = ActionKind::Tackle;
        world.step(PlayerInputs{}, actions);
        const MatchEvent* tackle = find(world, EventKind::Tackle);
        REQUIRE(tackle != nullptr);
        stats.won += tackle->value > 0.0f ? 1 : 0;
        stats.fouls += find(world, EventKind::FoulCandidate) != nullptr ? 1 : 0;
        if (tackle->value > 0.0f) {
            REQUIRE(world.possessor() != kCarrier);
        } else {
            REQUIRE(world.possessor() == kCarrier);
        }
    }
    return stats;
}

}

TEST_CASE("tackles are won more often by a better tackler against a weaker dribbler")
{
    const TackleStats strong = tackleMany(90, 20, 50);
    const TackleStats even = tackleMany(50, 50, 50);
    const TackleStats weak = tackleMany(10, 90, 50);
    REQUIRE(strong.won > even.won);
    REQUIRE(even.won > weak.won);
    REQUIRE(strong.won > weak.won * 2);
}

TEST_CASE("aggressive tacklers produce more foul candidates")
{
    const TackleStats calm = tackleMany(50, 50, 5);
    const TackleStats wild = tackleMany(50, 50, 95);
    REQUIRE(wild.fouls > calm.fouls * 2);
    REQUIRE(calm.fouls > 0);
}

TEST_CASE("a tackle out of reach, or on a teammate, does nothing")
{
    World world(profiles(), 1);
    parkOthers(world, {kKicker, kCarrier, kAwayMate});
    place(world, kCarrier, {0.0f, 0.0f}, 0.0f);
    place(world, kKicker, {-5.0f, 0.0f}, 0.0f);
    place(world, kAwayMate, {0.0f, 1.0f}, 0.0f);
    giveBall(world, kCarrier);

    PlayerActions actions{};
    actions[kKicker].kind = ActionKind::Tackle;
    actions[kAwayMate].kind = ActionKind::Tackle;
    world.step(PlayerInputs{}, actions);
    REQUIRE(world.events().empty());
    REQUIRE(world.possessor() == kCarrier);
}

TEST_CASE("a player on cooldown cannot kick, and a ball out of reach cannot be kicked")
{
    {
        World world(profiles(), 1);
        parkOthers(world, {kKicker});
        place(world, kKicker, {-20.0f, 0.0f}, 0.0f);
        giveBall(world, kKicker);

        PlayerActions actions{};
        actions[kKicker] = request(ActionKind::Pass, {0.0f, 0.0f}, 0.3f);
        world.step(PlayerInputs{}, actions);
        REQUIRE(find(world, EventKind::Pass) != nullptr);

        world.ball().position = {-20.4f, 0.0f, physics::ball::kRadius};
        world.ball().velocity = {};
        world.step(PlayerInputs{}, actions);
        REQUIRE(world.events().empty());
    }
    {
        World world(profiles(), 1);
        parkOthers(world, {kKicker});
        place(world, kKicker, {-20.0f, 0.0f}, 0.0f);
        world.ball().position = {-12.0f, 0.0f, physics::ball::kRadius};
        PlayerActions actions{};
        actions[kKicker] = request(ActionKind::Pass, {0.0f, 0.0f}, 0.3f);
        world.step(PlayerInputs{}, actions);
        REQUIRE(world.events().empty());
    }
}

TEST_CASE("a ball hitting a player above control height is deflected")
{
    World world(profiles(), 1);
    parkOthers(world, {kKicker});
    place(world, kKicker, {0.0f, 0.0f}, 0.0f);
    world.ball().position = {3.0f, 0.0f, 1.2f};
    world.ball().velocity = {-10.0f, 0.0f, 0.0f};
    for (int t = 0; t < 60; ++t) {
        world.step(PlayerInputs{});
        if (find(world, EventKind::Touch) != nullptr) {
            break;
        }
    }
    REQUIRE(world.possessor() == kNoPlayer);
    REQUIRE(world.ball().velocity.x > 0.0f);
    REQUIRE(world.lastToucher() == kKicker);
}

TEST_CASE("the human buttons drive pass, shoot and tackle")
{
    InputFrame pass;
    pass.buttons = InputFrame::Pass;
    InputFrame shoot;
    shoot.buttons = InputFrame::Shoot;
    InputFrame none;

    {
        World world(profiles(), 1);
        parkOthers(world, {kKicker, kMate});
        place(world, kKicker, {-10.0f, 0.0f}, 0.0f);
        place(world, kMate, {5.0f, 1.0f}, 0.0f);
        giveBall(world, kKicker);
        PlayerInputs inputs{};
        inputs[kKicker] = pass;
        for (int t = 0; t < 10; ++t) {
            world.step(inputs);
        }
        REQUIRE(world.possessor() == kKicker);
        inputs[kKicker] = none;
        world.step(inputs);
        const MatchEvent* e = find(world, EventKind::Pass);
        REQUIRE(e != nullptr);
        REQUIRE(e->other == kMate);
    }
    {
        World world(profiles(), 1);
        parkOthers(world, {kKicker});
        place(world, kKicker, {30.0f, 0.0f}, 0.0f);
        giveBall(world, kKicker);
        PlayerInputs inputs{};
        inputs[kKicker] = shoot;
        for (int t = 0; t < 45; ++t) {
            world.step(inputs);
        }
        inputs[kKicker] = none;
        world.step(inputs);
        const MatchEvent* e = find(world, EventKind::Shot);
        REQUIRE(e != nullptr);
        REQUIRE(e->value > possession::kShotSpeedMax * 0.9f);
        REQUIRE(world.ball().velocity.x > 0.0f);
    }
}

TEST_CASE("a shot into the net emits a goal, and the ball going over the line emits out of play once")
{
    {
        Profiles ps = profiles();
        ps[kKicker].attributes.finishing = 100;
        ps[kKicker].attributes.composure = 100;
        World world(ps, 2);
        parkOthers(world, {kKicker});
        place(world, kKicker, {40.0f, 0.0f}, 0.0f);
        giveBall(world, kKicker);
        PlayerActions actions{};
        actions[kKicker] = request(ActionKind::Shoot, {physics::pitch::kHalfLength, 0.0f}, 0.8f);
        int goals = 0;
        world.step(PlayerInputs{}, actions);
        for (int t = 0; t < 200; ++t) {
            world.step(PlayerInputs{});
            for (const MatchEvent& e : world.events()) {
                if (e.kind == EventKind::Goal) {
                    ++goals;
                    REQUIRE(e.side == physics::Side::Home);
                    REQUIRE(e.player == kKicker);
                    REQUIRE(e.line == physics::Line::GoalLinePlusX);
                }
            }
        }
        REQUIRE(goals <= 1);
        REQUIRE(world.ballDead());
    }
    {
        World world(profiles(), 2);
        parkOthers(world, {kKicker});
        place(world, kKicker, {0.0f, 30.0f}, 0.0f);
        giveBall(world, kKicker);
        PlayerActions actions{};
        actions[kKicker] = request(ActionKind::Pass, {0.0f, 60.0f}, 0.6f);
        world.step(PlayerInputs{}, actions);
        int outs = 0;
        for (int t = 0; t < 300; ++t) {
            world.step(PlayerInputs{});
            for (const MatchEvent& e : world.events()) {
                if (e.kind == EventKind::OutOfPlay) {
                    ++outs;
                    REQUIRE(e.line == physics::Line::TouchlinePlusY);
                    REQUIRE(e.player == kKicker);
                    REQUIRE(e.side == physics::Side::Home);
                }
            }
        }
        REQUIRE(outs == 1);
    }
}

namespace {

struct Trace {
    std::vector<std::uint64_t> hashes;
    std::vector<MatchEvent> events;
    std::vector<PlayerInputs> inputs;
};

// Everyone chases the ball and mashes buttons, so there is plenty to record.
PlayerInputs chase(const World& world, Rng& rng)
{
    PlayerInputs inputs{};
    for (std::size_t i = 0; i < kPlayerCount; ++i) {
        const Player& p = world.players()[i];
        const float sign = p.side == physics::Side::Home ? 1.0f : -1.0f;
        const Vec2 d = physics::xy(world.ball().position) - p.position;
        const float len = physics::length(d);
        const float scale = len > 1.0f ? 1.0f / len : 1.0f;
        inputs[i].moveY = d.x * scale * sign;
        inputs[i].moveX = -d.y * scale * sign;
        const std::uint32_t roll = rng.range(0, 29);
        const std::uint32_t buttons[] = {InputFrame::Pass, InputFrame::Shoot, InputFrame::Tackle, InputFrame::Through, InputFrame::Sprint};
        inputs[i].buttons = roll < 5 ? buttons[roll] : 0u;
    }
    return inputs;
}

Trace record(std::uint64_t seed, int ticks)
{
    World world(profiles(), seed);
    for (std::size_t i = 0; i < kPlayerCount; ++i) {
        world.players()[i].position = world.players()[i].position * 0.15f;
    }
    Rng rng(99);
    Trace trace;
    for (int t = 0; t < ticks; ++t) {
        const PlayerInputs inputs = chase(world, rng);
        world.step(inputs);
        trace.inputs.push_back(inputs);
        trace.hashes.push_back(world.hash());
        trace.events.insert(trace.events.end(), world.events().begin(), world.events().end());
    }
    return trace;
}

Trace replay(const std::vector<PlayerInputs>& inputs, std::uint64_t seed)
{
    World world(profiles(), seed);
    for (std::size_t i = 0; i < kPlayerCount; ++i) {
        world.players()[i].position = world.players()[i].position * 0.15f;
    }
    Trace trace;
    for (const PlayerInputs& in : inputs) {
        world.step(in);
        trace.hashes.push_back(world.hash());
        trace.events.insert(trace.events.end(), world.events().begin(), world.events().end());
    }
    return trace;
}

}

TEST_CASE("replaying a recorded input stream gives identical hashes and events")
{
    const Trace live = record(11, 3000);
    const Trace again = replay(live.inputs, 11);
    REQUIRE(again.hashes == live.hashes);
    REQUIRE(again.events == live.events);

    bool kinds[7] = {};
    for (const MatchEvent& e : live.events) {
        kinds[static_cast<int>(e.kind)] = true;
    }
    REQUIRE(kinds[static_cast<int>(EventKind::Touch)]);
    REQUIRE(kinds[static_cast<int>(EventKind::Pass)]);
    REQUIRE(kinds[static_cast<int>(EventKind::Shot)]);
    REQUIRE(kinds[static_cast<int>(EventKind::Tackle)]);

    REQUIRE(replay(live.inputs, 12).hashes != live.hashes);
}

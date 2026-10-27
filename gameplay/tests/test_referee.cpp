#include "nutmeg/core/dmath.h"
#include "nutmeg/core/rng.h"
#include "nutmeg/gameplay/actions.h"
#include "nutmeg/gameplay/referee.h"
#include "nutmeg/gameplay/world.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <functional>
#include <vector>

using namespace nutmeg;
using namespace nutmeg::gameplay;
using physics::Side;
using physics::Vec2;
using Catch::Approx;

namespace {

constexpr std::size_t kTackler = 10;
constexpr std::size_t kMate = 9;
constexpr std::size_t kCarrier = 11;
constexpr std::size_t kAwayMate = 12;

using Profiles = std::array<schema::PlayerProfile, kPlayerCount>;

Profiles profiles(int aggression = 50)
{
    Profiles result;
    for (std::size_t i = 0; i < kPlayerCount; ++i) {
        result[i].id = static_cast<std::uint32_t>(i);
        result[i].attributes.aggression = aggression;
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
        if (!kept) {
            world.players()[i].position = {-52.0f + 2.0f * static_cast<float>(n++), i < kPlayersPerSide ? -33.0f : 33.0f};
        }
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
    world.ball().position = {p.position.x + std::cos(p.facing) * 0.45f, p.position.y + std::sin(p.facing) * 0.45f, physics::ball::kRadius};
    world.ball().velocity = {};
    world.ball().spin = {};
    world.step(PlayerInputs{});
    REQUIRE(world.possessor() == index);
}

const MatchEvent* find(const std::vector<MatchEvent>& events, EventKind kind)
{
    for (const MatchEvent& e : events) {
        if (e.kind == kind) {
            return &e;
        }
    }
    return nullptr;
}

const MatchEvent* find(const World& world, EventKind kind) { return find(world.events(), kind); }

int count(const std::vector<MatchEvent>& events, EventKind kind)
{
    int n = 0;
    for (const MatchEvent& e : events) {
        n += e.kind == kind ? 1 : 0;
    }
    return n;
}

// Attackers on `side` at `attackerX`; the last two defenders at `last` and `secondLast`,
// everyone else far behind. X values are in the attackers' direction of play.
void arrange(World& world, Side side, float attackerX, float last, float secondLast)
{
    const float sign = side == Side::Home ? 1.0f : -1.0f;
    const std::size_t attackerBase = side == Side::Home ? 0 : kPlayersPerSide;
    const std::size_t defenderBase = side == Side::Home ? kPlayersPerSide : 0;
    for (std::size_t i = 0; i < kPlayersPerSide; ++i) {
        place(world, attackerBase + i, {-20.0f * sign, -20.0f + 3.0f * static_cast<float>(i)}, 0.0f);
        place(world, defenderBase + i, {-30.0f * sign, -20.0f + 3.0f * static_cast<float>(i)}, 0.0f);
    }
    place(world, attackerBase + 9, {attackerX * sign, 0.0f}, 0.0f);
    place(world, defenderBase + 0, {last * sign, 5.0f}, 0.0f);
    place(world, defenderBase + 1, {secondLast * sign, -5.0f}, 0.0f);
}

bool offside(const World& world, Side side, float ballX)
{
    const std::size_t index = (side == Side::Home ? 0 : kPlayersPerSide) + 9;
    return inOffsidePosition(world.players().data(), kPlayerCount, world.players()[index], ballX);
}

MatchEvent foulBy(std::size_t fouler, std::size_t victim)
{
    MatchEvent event;
    event.kind = EventKind::FoulCandidate;
    event.player = static_cast<std::uint32_t>(fouler);
    event.other = static_cast<std::uint32_t>(victim);
    return event;
}

Situation situationOf(const World& world, std::uint32_t possessor, std::uint64_t tick)
{
    return {world.players().data(), kPlayerCount, possessor, tick};
}

Referee refereeFor(std::uint64_t seed) { return Referee(Rng(seed).derive("referee"), kPlayerCount); }

}

TEST_CASE("offside position needs the attacker past the ball and the second-last defender in the opponents' half")
{
    World world(profiles(), 1);

    arrange(world, Side::Home, 35.0f, 45.0f, 30.0f);
    CHECK(offside(world, Side::Home, 20.0f));

    arrange(world, Side::Home, 30.0f, 45.0f, 30.0f);
    CHECK_FALSE(offside(world, Side::Home, 20.0f));

    arrange(world, Side::Home, 25.0f, 45.0f, 30.0f);
    CHECK_FALSE(offside(world, Side::Home, 20.0f));

    arrange(world, Side::Home, 45.0f, 45.0f, 30.0f);
    CHECK(offside(world, Side::Home, 20.0f));

    arrange(world, Side::Home, 35.0f, 45.0f, 30.0f);
    CHECK_FALSE(offside(world, Side::Home, 40.0f));

    arrange(world, Side::Home, -5.0f, -10.0f, -20.0f);
    CHECK_FALSE(offside(world, Side::Home, -30.0f));
}

TEST_CASE("offside is mirrored for the away side")
{
    World world(profiles(), 1);
    arrange(world, Side::Away, 35.0f, 45.0f, 30.0f);
    CHECK(offside(world, Side::Away, -20.0f));
    arrange(world, Side::Away, 25.0f, 45.0f, 30.0f);
    CHECK_FALSE(offside(world, Side::Away, -20.0f));
}

TEST_CASE("a sent-off defender no longer holds the offside line")
{
    World world(profiles(), 1);
    arrange(world, Side::Home, 25.0f, 45.0f, 30.0f);
    REQUIRE_FALSE(offside(world, Side::Home, 20.0f));
    world.players()[kAwayMate].sentOff = true;
    CHECK(offside(world, Side::Home, 20.0f));
}

namespace {

struct PassSetup {
    float mateX = 35.0f;
    bool opponentInPath = false;
};

World passWorld(const PassSetup& setup)
{
    Profiles ps = profiles();
    ps[kTackler].attributes.passing = 100;
    World world(ps, 3);
    parkOthers(world, {kTackler, kMate, kCarrier, kAwayMate});
    place(world, kTackler, {15.0f, 0.0f}, 0.0f);
    place(world, kMate, {setup.mateX, 3.0f}, dmath::kPi);
    place(world, kCarrier, {45.0f, 10.0f}, dmath::kPi);
    if (setup.opponentInPath) {
        place(world, kAwayMate, {25.0f, 1.5f}, dmath::kPi);
    } else {
        place(world, kAwayMate, {30.0f, -12.0f}, dmath::kPi);
    }
    giveBall(world, kTackler);

    PlayerActions actions{};
    actions[kTackler].kind = ActionKind::Pass;
    actions[kTackler].target = kMate;
    actions[kTackler].power = passPowerForDistance(physics::distance(world.players()[kTackler].position, world.players()[kMate].position));
    world.step(PlayerInputs{}, actions);
    return world;
}

bool runUntil(World& world, int ticks, const std::function<bool(const World&)>& done)
{
    for (int i = 0; i < ticks; ++i) {
        if (done(world)) {
            return true;
        }
        world.step(PlayerInputs{});
    }
    return done(world);
}

}

TEST_CASE("a pass to a teammate in an offside position is called when the teammate plays it")
{
    World world = passWorld({});
    const MatchEvent* offsideEvent = nullptr;
    REQUIRE(runUntil(world, 400, [&](const World& w) {
        offsideEvent = offsideEvent != nullptr ? offsideEvent : find(w, EventKind::Offside);
        return w.restart().has_value();
    }));

    REQUIRE(world.restart()->kind == RestartKind::IndirectFreeKick);
    CHECK(world.restart()->side == Side::Away);
    CHECK(world.restart()->spot.x == Approx(35.0f).margin(1.5f));
    const MatchEvent* event = find(world, EventKind::Offside);
    REQUIRE(event != nullptr);
    CHECK(event->player == kMate);
    CHECK(event->other == kTackler);
    CHECK(event->side == Side::Home);
}

TEST_CASE("a pass to a teammate who is onside is not called")
{
    World world = passWorld({.mateX = 25.0f});
    REQUIRE(runUntil(world, 400, [](const World& w) { return w.possessor() == kMate; }));
    CHECK_FALSE(world.restart().has_value());
    CHECK(world.possessor() == kMate);
}

TEST_CASE("offside is judged when the ball is played so a defender touching it first clears the offside attacker")
{
    World world = passWorld({.opponentInPath = true});
    bool called = false;
    for (int i = 0; i < 400; ++i) {
        world.step(PlayerInputs{});
        called = called || find(world, EventKind::Offside) != nullptr;
    }
    CHECK_FALSE(called);
    CHECK_FALSE(world.restart().has_value());
}

TEST_CASE("an offside attacker who does not touch the ball is not called")
{
    World world = passWorld({.mateX = 35.0f});
    world.players()[kMate].position = {35.0f, 30.0f};
    bool called = false;
    for (int i = 0; i < 300; ++i) {
        world.step(PlayerInputs{});
        called = called || find(world, EventKind::Offside) != nullptr;
    }
    CHECK_FALSE(called);
}

namespace {

struct Foul {
    World world;
    PlayerInputs inputs;
};

// Steps a tackle of kCarrier by kTackler with each seed until `wanted` comes up.
Foul firstTackle(int aggression, bool carrierRunning, Vec2 carrierAt, EventKind wanted)
{
    for (std::uint64_t seed = 0; seed < 4000; ++seed) {
        World world(profiles(aggression), seed);
        parkOthers(world, {kTackler, kCarrier});
        PlayerInputs inputs{};
        place(world, kTackler, {-30.0f, 20.0f}, 0.0f);
        place(world, kCarrier, carrierAt, dmath::kPi);
        giveBall(world, kCarrier);
        if (carrierRunning) {
            inputs[kCarrier].moveY = 1.0f;
            inputs[kCarrier].buttons = InputFrame::Sprint;
            for (int i = 0; i < 90; ++i) {
                world.step(inputs);
            }
            REQUIRE(world.possessor() == kCarrier);
        }
        place(world, kTackler, {world.players()[kCarrier].position.x - 1.1f, world.players()[kCarrier].position.y}, 0.0f);

        PlayerActions actions{};
        actions[kTackler].kind = ActionKind::Tackle;
        world.step(inputs, actions);
        if (find(world, wanted) != nullptr) {
            return {world, inputs};
        }
    }
    FAIL("no seed produced " << static_cast<int>(wanted));
    return {World(), {}};
}

}

TEST_CASE("a foul candidate becomes a foul and a direct free kick for the fouled side")
{
    const Foul foul = firstTackle(95, false, {0.0f, 0.0f}, EventKind::Foul);
    const World& world = foul.world;

    const MatchEvent* event = find(world, EventKind::Foul);
    REQUIRE(event != nullptr);
    CHECK(event->player == kTackler);
    CHECK(event->other == kCarrier);
    CHECK(event->side == Side::Home);
    CHECK(event->value == 0.0f);
    CHECK(find(world, EventKind::AdvantageGiven) == nullptr);

    REQUIRE(world.restart().has_value());
    CHECK(world.restart()->kind == RestartKind::DirectFreeKick);
    CHECK(world.restart()->side == Side::Away);
    CHECK(world.restart()->spot.x == Approx(world.players()[kCarrier].position.x).margin(0.05f));
    CHECK(world.possessor() == kNoPlayer);
}

TEST_CASE("the ball waits on the free kick spot and only the fouled side may play it")
{
    Foul foul = firstTackle(95, false, {0.0f, 0.0f}, EventKind::Foul);
    World& world = foul.world;
    const Vec2 spot = world.restart()->spot;

    PlayerActions rival{};
    rival[kTackler].kind = ActionKind::Pass;
    rival[kTackler].aim = {-30.0f, 0.0f};
    for (int i = 0; i < 40; ++i) {
        world.step(PlayerInputs{}, rival);
    }
    CHECK(world.restart().has_value());
    CHECK(world.possessor() == kNoPlayer);
    CHECK(physics::distance(spot, physics::xy(world.ball().position)) < 0.01f);

    PlayerActions taker{};
    taker[kCarrier].kind = ActionKind::Pass;
    taker[kCarrier].aim = {-20.0f, 0.0f};
    taker[kCarrier].power = 0.5f;
    world.step(PlayerInputs{}, taker);
    CHECK(find(world, EventKind::Pass) != nullptr);
    CHECK_FALSE(world.restart().has_value());
}

TEST_CASE("a foul inside the penalty area is a penalty for the fouled side")
{
    const Foul foul = firstTackle(95, false, {-45.0f, 0.0f}, EventKind::Foul);
    REQUIRE(foul.world.restart().has_value());
    CHECK(foul.world.restart()->kind == RestartKind::Penalty);
    CHECK(foul.world.restart()->side == Side::Away);
    CHECK(foul.world.restart()->spot == physics::pitch::penaltySpot(Side::Home));
}

TEST_CASE("an aggressive tackler is booked far more often than a calm one")
{
    const auto cards = [](int aggression) {
        int total = 0;
        const World world(profiles(aggression), 1);
        for (std::uint64_t seed = 0; seed < 400; ++seed) {
            Referee referee = refereeFor(seed);
            std::vector<MatchEvent> events{foulBy(kTackler, kCarrier)};
            referee.review(situationOf(world, kNoPlayer, 0), events);
            total += count(events, EventKind::YellowCard) + count(events, EventKind::RedCard);
        }
        return total;
    };
    const int wild = cards(100);
    const int calm = cards(1);
    CHECK(calm > 0);
    CHECK(wild > calm * 2);
}

TEST_CASE("a second yellow card is a red card and the player is sent off")
{
    bool sawSecondYellow = false;
    bool sawStraightRed = false;
    for (std::uint64_t seed = 0; seed < 200 && !(sawSecondYellow && sawStraightRed); ++seed) {
        const World world(profiles(100), seed);
        Referee referee = refereeFor(seed);
        int yellows = 0;
        const MatchEvent* red = nullptr;
        std::vector<MatchEvent> all;
        for (std::uint64_t tick = 0; tick < 60 && red == nullptr; ++tick) {
            std::vector<MatchEvent> events{foulBy(kTackler, kCarrier)};
            referee.review(situationOf(world, kNoPlayer, tick), events);
            all.insert(all.end(), events.begin(), events.end());
            red = find(all, EventKind::RedCard);
        }
        REQUIRE(red != nullptr);
        yellows = count(all, EventKind::YellowCard);
        CHECK(red->player == kTackler);
        CHECK(referee.sentOff(kTackler));
        if (red->value == 2.0f) {
            CHECK(yellows == 2);
            sawSecondYellow = true;
        } else {
            CHECK(yellows < 2);
            sawStraightRed = true;
        }

        const int fouls = count(all, EventKind::Foul);
        std::vector<MatchEvent> after{foulBy(kTackler, kCarrier)};
        CHECK_FALSE(referee.review(situationOf(world, kNoPlayer, 100), after).has_value());
        CHECK(count(after, EventKind::Foul) == 0);
        CHECK(fouls > 0);
    }
    CHECK(sawSecondYellow);
    CHECK(sawStraightRed);
}

TEST_CASE("a red card sends the player off the pitch and out of the game")
{
    Foul foul = firstTackle(100, false, {0.0f, 0.0f}, EventKind::RedCard);
    World& world = foul.world;
    REQUIRE(world.players()[kTackler].sentOff);
    CHECK(world.referee().sentOff(kTackler));
    CHECK(std::abs(world.players()[kTackler].position.y) > physics::pitch::kHalfWidth);

    const Vec2 off = world.players()[kTackler].position;
    PlayerInputs inputs{};
    inputs[kTackler].moveY = 1.0f;
    inputs[kTackler].moveX = 1.0f;
    for (int i = 0; i < 60; ++i) {
        world.step(inputs);
    }
    CHECK(world.players()[kTackler].position == off);
}

namespace {

struct AdvantageCase {
    World world;
    Referee referee;
    std::vector<MatchEvent> events;
    std::optional<Restart> restart;
};

// The fouled away carrier is running at `speed` towards the home goal.
AdvantageCase advantageFoul(int aggression, float speed, Vec2 at, const std::function<bool(const std::vector<MatchEvent>&, bool)>& accept)
{
    for (std::uint64_t seed = 0; seed < 400; ++seed) {
        World world(profiles(aggression), seed);
        place(world, kTackler, {at.x - 1.0f, at.y}, 0.0f);
        place(world, kCarrier, at, dmath::kPi);
        world.players()[kCarrier].velocity = {-speed, 0.0f};
        Referee referee = refereeFor(seed);
        std::vector<MatchEvent> events{foulBy(kTackler, kCarrier)};
        const std::optional<Restart> restart = referee.review(situationOf(world, kCarrier, 10), events);
        if (accept(events, restart.has_value())) {
            return {world, referee, events, restart};
        }
    }
    FAIL("no seed gave the wanted ruling");
    return {World(), refereeFor(0), {}, std::nullopt};
}

bool played(const std::vector<MatchEvent>& events, bool) { return find(events, EventKind::AdvantageGiven) != nullptr; }

}

TEST_CASE("advantage is played when the fouled carrier keeps the ball and runs on")
{
    AdvantageCase c = advantageFoul(50, 6.0f, {10.0f, 0.0f}, played);
    CHECK_FALSE(c.restart.has_value());
    CHECK(c.referee.advantageActive());
    const MatchEvent* foul = find(c.events, EventKind::Foul);
    REQUIRE(foul != nullptr);
    CHECK(foul->value == 1.0f);

    std::vector<MatchEvent> quiet;
    CHECK_FALSE(c.referee.review(situationOf(c.world, kCarrier, 60), quiet).has_value());
    CHECK(c.referee.advantageActive());

    std::vector<MatchEvent> expiry;
    CHECK_FALSE(c.referee.review(situationOf(c.world, kCarrier, 10 + referee::kAdvantageTicks), expiry).has_value());
    const MatchEvent* ended = find(expiry, EventKind::AdvantageEnded);
    REQUIRE(ended != nullptr);
    CHECK(ended->value == 1.0f);
    CHECK_FALSE(c.referee.advantageActive());
}

TEST_CASE("the foul is called back when the fouled side loses the ball during advantage")
{
    AdvantageCase c = advantageFoul(50, 6.0f, {10.0f, 0.0f}, played);
    std::vector<MatchEvent> events;
    const std::optional<Restart> restart = c.referee.review(situationOf(c.world, 3, 40), events);
    REQUIRE(restart.has_value());
    CHECK(restart->kind == RestartKind::DirectFreeKick);
    CHECK(restart->side == Side::Away);
    CHECK(restart->spot.x == Approx(10.0f));
    const MatchEvent* ended = find(events, EventKind::AdvantageEnded);
    REQUIRE(ended != nullptr);
    CHECK(ended->value == 0.0f);
    CHECK_FALSE(c.referee.advantageActive());
}

TEST_CASE("the foul is called back when the ball goes out during advantage")
{
    AdvantageCase c = advantageFoul(50, 6.0f, {10.0f, 0.0f}, played);
    MatchEvent out;
    out.kind = EventKind::OutOfPlay;
    out.tick = 30;
    std::vector<MatchEvent> events{out};
    CHECK(c.referee.review(situationOf(c.world, kNoPlayer, 30), events).has_value());
    CHECK(find(events, EventKind::AdvantageEnded)->value == 0.0f);
}

TEST_CASE("a goal during advantage lets play stand")
{
    AdvantageCase c = advantageFoul(50, 6.0f, {10.0f, 0.0f}, played);
    MatchEvent goal;
    goal.kind = EventKind::Goal;
    goal.tick = 30;
    std::vector<MatchEvent> events{goal};
    CHECK_FALSE(c.referee.review(situationOf(c.world, kNoPlayer, 30), events).has_value());
    CHECK(find(events, EventKind::AdvantageEnded)->value == 1.0f);
}

TEST_CASE("a card for a foul played on with advantage is shown when the advantage ends")
{
    AdvantageCase c = advantageFoul(100, 6.0f, {10.0f, 0.0f}, [](const std::vector<MatchEvent>& events, bool stopped) {
        return !stopped && find(events, EventKind::AdvantageGiven) != nullptr;
    });
    CHECK(find(c.events, EventKind::YellowCard) == nullptr);

    std::vector<MatchEvent> expiry;
    c.referee.review(situationOf(c.world, kCarrier, 10 + referee::kAdvantageTicks), expiry);
    const bool booked = find(expiry, EventKind::YellowCard) != nullptr || find(expiry, EventKind::RedCard) != nullptr;
    CHECK(booked == (c.referee.yellowCards(kTackler) > 0 || c.referee.sentOff(kTackler)));
}

TEST_CASE("play is stopped at once for a slow carrier, a penalty area foul and a red card")
{
    const AdvantageCase slow = advantageFoul(50, 0.5f, {10.0f, 0.0f}, [](const std::vector<MatchEvent>&, bool stopped) { return stopped; });
    CHECK(find(slow.events, EventKind::AdvantageGiven) == nullptr);
    CHECK(find(slow.events, EventKind::Foul)->value == 0.0f);

    const AdvantageCase box = advantageFoul(50, 6.0f, {-45.0f, 0.0f}, [](const std::vector<MatchEvent>&, bool stopped) { return stopped; });
    CHECK(box.restart->kind == RestartKind::Penalty);
    CHECK(find(box.events, EventKind::AdvantageGiven) == nullptr);

    const AdvantageCase red = advantageFoul(100, 6.0f, {10.0f, 0.0f}, [](const std::vector<MatchEvent>& events, bool) { return find(events, EventKind::RedCard) != nullptr; });
    CHECK(red.restart.has_value());
    CHECK(find(red.events, EventKind::AdvantageGiven) == nullptr);
}

TEST_CASE("advantage runs end to end in the world for a carrier who keeps running")
{
    Foul foul = firstTackle(95, true, {20.0f, 0.0f}, EventKind::AdvantageGiven);
    World& world = foul.world;
    CHECK_FALSE(world.restart().has_value());
    CHECK(world.possessor() == kCarrier);

    bool ended = false;
    for (int i = 0; i < referee::kAdvantageTicks + 5 && !ended; ++i) {
        world.step(foul.inputs);
        if (const MatchEvent* e = find(world, EventKind::AdvantageEnded)) {
            ended = true;
            CHECK(e->value == 1.0f);
        }
    }
    CHECK(ended);
    CHECK_FALSE(world.restart().has_value());
}

TEST_CASE("the referee is deterministic for a seed")
{
    const auto run = [] {
        Foul foul = firstTackle(95, false, {0.0f, 0.0f}, EventKind::Foul);
        for (int i = 0; i < 120; ++i) {
            foul.world.step(PlayerInputs{});
        }
        return foul.world.hash();
    };
    CHECK(run() == run());
}

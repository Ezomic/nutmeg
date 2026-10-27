#include "nutmeg/ai/bot.h"
#include "nutmeg/ai/setpiece.h"

#include <catch2/catch_test_macros.hpp>

using namespace nutmeg;
using namespace nutmeg::ai;
using gameplay::EventKind;
using gameplay::Restart;
using gameplay::RestartKind;
using physics::Side;
using physics::Vec2;

namespace {

struct Match {
    gameplay::World world{1};
    Decider home;
    Decider away;

    void step()
    {
        gameplay::PlayerInputs inputs{};
        gameplay::PlayerActions actions{};
        drive(world, Side::Home, Formation::F442, Mentality::Balanced, BallControl::Loose, inputs, actions, home);
        drive(world, Side::Away, Formation::F442, Mentality::Balanced, BallControl::Loose, inputs, actions, away);
        world.step(inputs, actions);
    }
};

Restart restartAt(RestartKind kind, Side side, Vec2 spot)
{
    Restart restart;
    restart.kind = kind;
    restart.side = side;
    restart.spot = spot;
    return restart;
}

struct Taken {
    bool taken = false;
    bool pass = false;
    bool shot = false;
    std::uint32_t taker = gameplay::kNoPlayer;
    int ticks = 0;
};

Taken runUntilTaken(Match& match, int limit = 400)
{
    Taken result;
    result.taker = match.world.restart() ? match.world.restart()->taker : gameplay::kNoPlayer;
    for (; result.ticks < limit && !result.taken; ++result.ticks) {
        match.step();
        for (const gameplay::MatchEvent& e : match.world.events()) {
            result.pass = result.pass || (e.kind == EventKind::Pass && e.player == result.taker);
            result.shot = result.shot || (e.kind == EventKind::Shot && e.player == result.taker);
        }
        result.taken = !match.world.restart().has_value();
    }
    return result;
}

Taken takeRestart(const Restart& restart)
{
    Match match;
    match.world.restartPlay(restart);
    return runUntilTaken(match);
}

}

TEST_CASE("the AI takes a kickoff with a pass")
{
    const Taken t = takeRestart(restartAt(RestartKind::Kickoff, Side::Home, {0.0f, 0.0f}));
    REQUIRE(t.taken);
    REQUIRE(t.pass);
    REQUIRE(t.ticks >= setpiece_tuning::kTakeDelayTicks);
}

TEST_CASE("the AI takes a throw-in with a throw")
{
    const Taken t = takeRestart(restartAt(RestartKind::ThrowIn, Side::Away, {-10.0f, 33.95f}));
    REQUIRE(t.taken);
    REQUIRE(t.pass);
}

TEST_CASE("the AI keeper takes a goal kick")
{
    const Taken t = takeRestart(restartAt(RestartKind::GoalKick, Side::Home, {-49.75f, 4.0f}));
    REQUIRE(t.taken);
    REQUIRE(t.taker == 0);
    REQUIRE(t.pass);
}

TEST_CASE("the AI crosses a corner into the box")
{
    Match match;
    match.world.restartPlay(restartAt(RestartKind::Corner, Side::Away, {-52.45f, 33.95f}));
    const Taken t = runUntilTaken(match);
    REQUIRE(t.taken);
    REQUIRE(t.pass);
    REQUIRE(match.world.ball().velocity.z > 1.0f);
}

TEST_CASE("the AI sends attackers into the box for a corner")
{
    Match match;
    for (std::size_t i = 1; i < gameplay::kPlayersPerSide; ++i) {
        match.world.players()[gameplay::kPlayersPerSide + i].position = {-20.0f, -25.0f + 5.0f * static_cast<float>(i)};
    }
    match.world.restartPlay(restartAt(RestartKind::Corner, Side::Away, {-52.45f, 33.95f}));
    for (int t = 0; t < 170; ++t) {
        match.step();
    }
    int inBox = 0;
    for (const gameplay::Player& p : match.world.players()) {
        inBox += p.side == Side::Away && physics::pitch::inPenaltyArea(p.position, Side::Home) ? 1 : 0;
    }
    REQUIRE(inBox >= 3);
}

TEST_CASE("the AI shoots a direct free kick in range")
{
    const Taken t = takeRestart(restartAt(RestartKind::DirectFreeKick, Side::Home, {28.0f, 4.0f}));
    REQUIRE(t.taken);
    REQUIRE(t.shot);
}

TEST_CASE("the AI plays a direct free kick from its own half short")
{
    const Taken t = takeRestart(restartAt(RestartKind::DirectFreeKick, Side::Home, {-25.0f, 10.0f}));
    REQUIRE(t.taken);
    REQUIRE(t.pass);
    REQUIRE_FALSE(t.shot);
}

TEST_CASE("the AI plays an indirect free kick without shooting")
{
    const Taken t = takeRestart(restartAt(RestartKind::IndirectFreeKick, Side::Away, {-30.0f, 0.0f}));
    REQUIRE(t.taken);
    REQUIRE_FALSE(t.shot);
}

TEST_CASE("the AI takes a penalty with a shot")
{
    const Taken t = takeRestart(restartAt(RestartKind::Penalty, Side::Away, physics::pitch::penaltySpot(Side::Home)));
    REQUIRE(t.taken);
    REQUIRE(t.shot);
}

TEST_CASE("a ball kicked out is restarted by the AI with no human input")
{
    Match match;
    gameplay::World& world = match.world;
    world.players()[2].position = {10.0f, 30.0f};
    world.ball().position = {10.0f, 30.4f, physics::ball::kRadius};
    world.ball().velocity = {0.0f, 14.0f, 0.0f};
    bool thrown = false;
    for (int t = 0; t < 600 && !thrown; ++t) {
        match.step();
        thrown = world.lastToucher() != gameplay::kNoPlayer && world.restart() && world.restart()->kind == RestartKind::ThrowIn;
    }
    REQUIRE((world.pendingRestart().has_value() || world.restart().has_value()));
    for (int t = 0; t < 400; ++t) {
        match.step();
    }
    REQUIRE_FALSE(world.restart().has_value());
    REQUIRE_FALSE(world.pendingRestart().has_value());
}

TEST_CASE("the same AI restart gives the same hash")
{
    const auto run = [] {
        Match match;
        match.world.restartPlay(restartAt(RestartKind::Corner, Side::Home, {52.45f, -33.95f}));
        for (int t = 0; t < 300; ++t) {
            match.step();
        }
        return match.world.hash();
    };
    REQUIRE(run() == run());
}

#include "nutmeg/ai/bot.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace nutmeg;
using namespace nutmeg::ai;
using physics::Side;
using physics::Vec2;

namespace {

constexpr int kTicks = 60 * 60;

// Pinned above every player so the shape is measured against a ball nobody can play.
constexpr float kHeldAloft = 5.0f;

struct Run {
    std::uint64_t hash = 0;
    ShapeMetrics home;
    ShapeMetrics away;
};

std::array<Vec2, kOutfielders> outfield(const gameplay::World& world, Side side)
{
    std::array<Vec2, kOutfielders> out{};
    const std::size_t base = side == Side::Home ? 0 : gameplay::kPlayersPerSide;
    for (std::size_t i = 0; i < kOutfielders; ++i) {
        out[i] = world.players()[base + 1 + i].position;
    }
    return out;
}

Run play(Formation home, Formation away, Mentality homeMentality, Mentality awayMentality, BallControl control)
{
    gameplay::World world;
    Run run;
    for (int t = 0; t < kTicks; ++t) {
        gameplay::PlayerInputs inputs{};
        drive(world, Side::Home, home, homeMentality, control, inputs);
        drive(world, Side::Away, away, awayMentality, control, inputs);
        world.ball() = physics::Ball{};
        world.ball().position.z = kHeldAloft;
        world.step(inputs);
    }
    run.hash = world.hash();
    run.home = measure(Side::Home, home, outfield(world, Side::Home));
    run.away = measure(Side::Away, away, outfield(world, Side::Away));
    return run;
}

}

TEST_CASE("sixty seconds of idle-ball AI keeps both teams in shape")
{
    for (const BallControl control : {BallControl::Loose, BallControl::Home, BallControl::Away}) {
        const Run run = play(Formation::F442, Formation::F433, Mentality::Balanced, Mentality::Balanced, control);
        for (const ShapeMetrics& m : {run.home, run.away}) {
            REQUIRE(m.blockLength > 20.0f);
            REQUIRE(m.blockLength < 45.0f);
            REQUIRE(m.blockWidth > 30.0f);
            REQUIRE(m.blockWidth < 64.0f);
            REQUIRE(m.lineHeight > -45.0f);
            REQUIRE(m.lineHeight < -20.0f);
        }
    }
}

TEST_CASE("the same AI run twice gives identical hashes")
{
    const Run a = play(Formation::F352, Formation::F4231, Mentality::Attacking, Mentality::Defensive, BallControl::Away);
    const Run b = play(Formation::F352, Formation::F4231, Mentality::Attacking, Mentality::Defensive, BallControl::Away);
    REQUIRE(a.hash == b.hash);
}

TEST_CASE("an attacking team holds a higher line than a defensive one on the pitch")
{
    const Run attacking = play(Formation::F442, Formation::F442, Mentality::Attacking, Mentality::Attacking, BallControl::Loose);
    const Run defensive = play(Formation::F442, Formation::F442, Mentality::Defensive, Mentality::Defensive, BallControl::Loose);
    REQUIRE(attacking.home.lineHeight > defensive.home.lineHeight + 5.0f);
    REQUIRE(attacking.away.lineHeight > defensive.away.lineHeight + 5.0f);
}

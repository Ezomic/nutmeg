#include "nutmeg/debugview/demo_scene.h"
#include "nutmeg/debugview/view_transform.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <utility>

using namespace nutmeg::debugview;
using Catch::Approx;

TEST_CASE("the centre spot lands in the middle of the window")
{
    const ViewTransform view(1280, 720);
    const ScreenPoint p = view.toScreen({0.0f, 0.0f});
    REQUIRE(p.x == Approx(640.0f));
    REQUIRE(p.y == Approx(360.0f));
}

TEST_CASE("world y grows upwards and x to the right on screen")
{
    const ViewTransform view(1280, 720);
    REQUIRE(view.toScreen({10.0f, 0.0f}).x > view.toScreen({0.0f, 0.0f}).x);
    REQUIRE(view.toScreen({0.0f, 10.0f}).y < view.toScreen({0.0f, 0.0f}).y);
}

TEST_CASE("the whole pitch fits the window with the margin")
{
    for (const auto [w, h] : {std::pair{1280, 720}, std::pair{800, 800}, std::pair{400, 900}}) {
        const ViewTransform view(w, h);
        const ScreenPoint corner = view.toScreen({kPitchLength / 2.0f, kPitchWidth / 2.0f});
        const ScreenPoint opposite = view.toScreen({-kPitchLength / 2.0f, -kPitchWidth / 2.0f});
        REQUIRE(opposite.x >= 0.0f);
        REQUIRE(opposite.y <= static_cast<float>(h));
        REQUIRE(corner.x <= static_cast<float>(w));
        REQUIRE(corner.y >= 0.0f);
    }
}

TEST_CASE("the scale is the same on both axes")
{
    const ViewTransform view(1000, 1000);
    const ScreenPoint a = view.toScreen({0.0f, 0.0f});
    const ScreenPoint b = view.toScreen({1.0f, 0.0f});
    const ScreenPoint c = view.toScreen({0.0f, 1.0f});
    REQUIRE(b.x - a.x == Approx(a.y - c.y));
    REQUIRE(b.x - a.x == Approx(view.pixelsPerMetre()));
}

TEST_CASE("screen to world undoes world to screen")
{
    const ViewTransform view(1280, 720);
    const Vec2 world{-23.5f, 14.25f};
    const Vec2 back = view.toWorld(view.toScreen(world));
    REQUIRE(back.x == Approx(world.x));
    REQUIRE(back.y == Approx(world.y));
}

TEST_CASE("a zero-sized window does not divide by zero")
{
    const ViewTransform view(0, 0);
    REQUIRE(view.pixelsPerMetre() > 0.0f);
}

TEST_CASE("the demo scene keeps players and the ball on the pitch and is repeatable")
{
    for (double t = 0.0; t < 30.0; t += 0.5) {
        const RenderState state = demoScene(t);
        for (const auto& team : {state.home, state.away}) {
            for (const PlayerView& player : team) {
                REQUIRE(player.position.x >= -kPitchLength / 2.0f);
                REQUIRE(player.position.x <= kPitchLength / 2.0f);
                REQUIRE(player.position.y >= -kPitchWidth / 2.0f);
                REQUIRE(player.position.y <= kPitchWidth / 2.0f);
            }
        }
        REQUIRE(state.ball.height >= 0.0f);
    }
    REQUIRE(demoScene(3.0).ball.position.x == demoScene(3.0).ball.position.x);
    REQUIRE(demoScene(1.0).home[0].number == 1);
    REQUIRE(demoScene(1.0).home[10].number == 11);
}

#include "nutmeg/ai/bot.h"
#include "nutmeg/ai/team_shape.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>

using namespace nutmeg;
using namespace nutmeg::ai;
using physics::Side;
using physics::Vec2;
namespace pitch = physics::pitch;

namespace {

constexpr std::array<Formation, 6> kFormations = {
    Formation::F442, Formation::F433, Formation::F352, Formation::F4231, Formation::F532, Formation::F343};

std::array<Vec2, kOutfielders> anchorsOf(Formation f)
{
    std::array<Vec2, kOutfielders> out{};
    for (std::size_t i = 0; i < kOutfielders; ++i) {
        out[i] = slots(f)[i].anchor;
    }
    return out;
}

std::array<Vec2, kOutfielders> mirrored(const std::array<Vec2, kOutfielders>& in)
{
    std::array<Vec2, kOutfielders> out{};
    for (std::size_t i = 0; i < kOutfielders; ++i) {
        out[i] = {-in[i].x, -in[i].y};
    }
    return out;
}

TeamInput inputAt(Side side, Vec2 ball, BallControl control, Mentality mentality = Mentality::Balanced, Formation f = Formation::F442)
{
    TeamInput in;
    in.side = side;
    in.formation = f;
    in.mentality = mentality;
    in.control = control;
    in.ball = ball;
    const auto own = anchorsOf(f);
    in.positions = side == Side::Home ? own : mirrored(own);
    in.opponents = side == Side::Home ? mirrored(anchorsOf(Formation::F442)) : anchorsOf(Formation::F442);
    return in;
}

std::array<Vec2, kOutfielders> pointsOf(const TeamIntent& intent)
{
    std::array<Vec2, kOutfielders> out{};
    for (std::size_t i = 0; i < kOutfielders; ++i) {
        out[i] = intent.targets[i].point;
    }
    return out;
}

}

TEST_CASE("every formation has ten outfield slots on its own half of the pitch")
{
    for (const Formation f : kFormations) {
        const auto& layout = slots(f);
        REQUIRE(layout.size() == 10);
        for (const Slot& s : layout) {
            REQUIRE(pitch::contains(s.anchor));
            REQUIRE(s.anchor.x < 0.0f);
        }
    }
}

TEST_CASE("formation names parse")
{
    REQUIRE(parseFormation("4231") == Formation::F4231);
    REQUIRE(parseFormation("999") == std::nullopt);
    REQUIRE(parseMentality("defensive") == Mentality::Defensive);
    REQUIRE(parseMentality("") == std::nullopt);
}

TEST_CASE("planned targets stay inside the pitch for every formation and ball position")
{
    for (const Formation f : kFormations) {
        for (const float bx : {-52.5f, 0.0f, 52.5f}) {
            for (const float by : {-34.0f, 0.0f, 34.0f}) {
                for (const BallControl c : {BallControl::Home, BallControl::Away, BallControl::Loose}) {
                    const TeamIntent intent = planTeam(inputAt(Side::Home, {bx, by}, c, Mentality::Attacking, f));
                    for (const PlayerTarget& t : intent.targets) {
                        REQUIRE(pitch::contains(t.point));
                        REQUIRE(t.urgency >= 0.0f);
                        REQUIRE(t.urgency <= 1.0f);
                    }
                }
            }
        }
    }
}

TEST_CASE("the away side is the home side turned about the centre spot")
{
    for (const Formation f : kFormations) {
        const Vec2 ball{12.0f, -9.0f};
        const TeamIntent home = planTeam(inputAt(Side::Home, ball, BallControl::Away, Mentality::Balanced, f));
        const TeamIntent away = planTeam(inputAt(Side::Away, {-ball.x, -ball.y}, BallControl::Home, Mentality::Balanced, f));
        REQUIRE(home.phase == away.phase);
        for (std::size_t i = 0; i < kOutfielders; ++i) {
            REQUIRE(away.targets[i].point.x == -home.targets[i].point.x);
            REQUIRE(away.targets[i].point.y == -home.targets[i].point.y);
            REQUIRE(away.targets[i].urgency == home.targets[i].urgency);
            REQUIRE(away.roles[i] == home.roles[i]);
        }
    }
}

TEST_CASE("the block follows the ball and the shift is bounded")
{
    const auto at = [](float x) {
        return measure(Side::Home, Formation::F442, pointsOf(planTeam(inputAt(Side::Home, {x, 0.0f}, BallControl::Loose))));
    };
    const ShapeMetrics back = at(-52.5f);
    const ShapeMetrics middle = at(0.0f);
    const ShapeMetrics front = at(52.5f);
    REQUIRE(back.lineHeight < middle.lineHeight);
    REQUIRE(middle.lineHeight < front.lineHeight);
    const float span = front.lineHeight - back.lineHeight;
    REQUIRE(span <= tuning::kMaxShiftX - tuning::kMinShiftX + 1e-3f);
    REQUIRE(span > 10.0f);
}

TEST_CASE("the block leans towards the side the ball is on")
{
    const auto centroidY = [](float by) {
        const TeamIntent intent = planTeam(inputAt(Side::Home, {0.0f, by}, BallControl::Loose));
        float sum = 0.0f;
        for (const PlayerTarget& t : intent.targets) sum += t.point.y;
        return sum / 10.0f;
    };
    REQUIRE(centroidY(-20.0f) < centroidY(0.0f));
    REQUIRE(centroidY(20.0f) > centroidY(0.0f));
}

TEST_CASE("an attacking mentality sits higher than a defensive one")
{
    for (const BallControl c : {BallControl::Home, BallControl::Away, BallControl::Loose}) {
        const auto height = [c](Mentality m) {
            return measure(Side::Home, Formation::F442, pointsOf(planTeam(inputAt(Side::Home, {0.0f, 0.0f}, c, m)))).lineHeight;
        };
        REQUIRE(height(Mentality::Attacking) > height(Mentality::Balanced));
        REQUIRE(height(Mentality::Balanced) > height(Mentality::Defensive));
    }
}

TEST_CASE("phase follows possession and changes the shape")
{
    REQUIRE(phaseOf(Side::Home, BallControl::Home) == Phase::Attacking);
    REQUIRE(phaseOf(Side::Away, BallControl::Home) == Phase::Defending);
    REQUIRE(phaseOf(Side::Away, BallControl::Loose) == Phase::Loose);

    const auto shape = [](BallControl c) {
        return measure(Side::Home, Formation::F442, pointsOf(planTeam(inputAt(Side::Home, {0.0f, 0.0f}, c))));
    };
    const ShapeMetrics attack = shape(BallControl::Home);
    const ShapeMetrics defend = shape(BallControl::Away);
    REQUIRE(attack.blockWidth > defend.blockWidth);
    REQUIRE(attack.lineHeight > defend.lineHeight);
}

TEST_CASE("only the nearest defenders press the ball")
{
    TeamInput in = inputAt(Side::Home, {-25.0f, 5.0f}, BallControl::Away);
    const TeamIntent intent = planTeam(in);
    std::array<std::pair<float, std::size_t>, kOutfielders> byDistance{};
    for (std::size_t i = 0; i < kOutfielders; ++i) {
        byDistance[i] = {physics::distance(in.positions[i], in.ball), i};
    }
    std::sort(byDistance.begin(), byDistance.end());

    int pressers = 0;
    for (const Role r : intent.roles) pressers += r == Role::Press;
    REQUIRE(pressers <= 2);
    REQUIRE(pressers >= 1);
    REQUIRE(intent.roles[byDistance[0].second] == Role::Press);
    REQUIRE(intent.targets[byDistance[0].second].urgency == tuning::kPressUrgency);
    for (std::size_t n = 2; n < kOutfielders; ++n) {
        REQUIRE(intent.roles[byDistance[n].second] != Role::Press);
    }

    const TeamIntent attacking = planTeam(inputAt(Side::Home, {-25.0f, 5.0f}, BallControl::Home));
    for (const Role r : attacking.roles) REQUIRE(r != Role::Press);
}

TEST_CASE("nobody presses a ball that is out of range")
{
    const TeamIntent intent = planTeam(inputAt(Side::Home, {52.0f, 0.0f}, BallControl::Away));
    for (const Role r : intent.roles) REQUIRE(r != Role::Press);
}

TEST_CASE("attackers stay level with the offside line")
{
    TeamInput in = inputAt(Side::Home, {2.0f, 0.0f}, BallControl::Home, Mentality::Attacking);
    in.offsideLineX = 8.0f;
    const TeamIntent intent = planTeam(in);
    const auto& layout = slots(in.formation);
    bool capped = false;
    for (std::size_t i = 0; i < kOutfielders; ++i) {
        if (layout[i].line == Line::Attack) {
            REQUIRE(intent.targets[i].point.x <= 8.0f);
            capped = capped || intent.targets[i].point.x > 7.0f;
        }
    }
    REQUIRE(capped);

    in.offsideLineX = 0.0f;
    in.ball = {-30.0f, 0.0f};
    const TeamIntent deep = planTeam(in);
    for (std::size_t i = 0; i < kOutfielders; ++i) {
        if (layout[i].line == Line::Attack) {
            REQUIRE(deep.targets[i].point.x <= 0.0f);
        }
    }
}

TEST_CASE("a target becomes a stick in the frame World expects")
{
    gameplay::World world;
    const gameplay::Player& home = world.players()[1];
    const gameplay::Player& away = world.players()[12];

    const InputFrame ahead = moveToward(home, {home.position + Vec2{10.0f, 0.0f}, 0.0f});
    REQUIRE(ahead.moveY > 0.99f);
    REQUIRE(ahead.moveX == 0.0f);
    const InputFrame toRight = moveToward(home, {home.position + Vec2{0.0f, -10.0f}, 0.0f});
    REQUIRE(toRight.moveX > 0.99f);

    const InputFrame awayAhead = moveToward(away, {away.position + Vec2{-10.0f, 0.0f}, 0.0f});
    REQUIRE(awayAhead.moveY > 0.99f);

    const InputFrame there = moveToward(home, {home.position, 1.0f});
    REQUIRE(there == InputFrame{});
    const InputFrame sprint = moveToward(home, {home.position + Vec2{10.0f, 0.0f}, 0.9f});
    REQUIRE((sprint.buttons & InputFrame::Sprint) != 0);
    const InputFrame jog = moveToward(home, {home.position + Vec2{10.0f, 0.0f}, 0.5f});
    REQUIRE((jog.buttons & InputFrame::Sprint) == 0);
}

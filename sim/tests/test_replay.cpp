#include "nutmeg/sim/replay.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>

using namespace nutmeg;
using gameplay::EventKind;

namespace {

constexpr std::size_t kShooter = Simulation::kControlledPlayer;
constexpr std::uint64_t kShotTicks = 400;

// The match starts, then Home is given a free kick at the spot; the shooter's stick input is
// a long shot, and the lifecycle takes the kick itself if nobody does.
Simulation stagedShot(std::uint64_t seed, float shooterY, float shooterX = 28.0f)
{
    Simulation sim(seed);
    sim.tick(InputFrame{});
    gameplay::Restart restart;
    restart.kind = gameplay::RestartKind::DirectFreeKick;
    restart.side = physics::Side::Home;
    restart.spot = {shooterX, shooterY};
    sim.world().players()[kShooter].position = restart.spot;
    sim.world().restartPlay(restart);
    return sim;
}

InputStream shotStream(float stickY = 0.0f)
{
    InputStream stream;
    for (std::uint64_t i = 0; i < kShotTicks; ++i) {
        InputFrame frame;
        frame.buttons = i == 1 ? InputFrame::Shoot : 0u;
        frame.moveY = i == 1 ? stickY : 0.0f;
        stream.push(frame);
    }
    return stream;
}

bool hasHighlight(const std::vector<Highlight>& highlights, HighlightKind kind)
{
    return std::any_of(highlights.begin(), highlights.end(), [&](const Highlight& h) { return h.kind == kind; });
}

struct Staged {
    Simulation start;
    MatchRecording recording;
};

Staged findShotWith(HighlightKind kind, std::uint64_t interval)
{
    for (std::uint64_t seed = 1; seed < 200; ++seed) {
        for (const float x : {28.0f, 14.0f}) {
            for (const float y : {0.0f, 6.0f, -6.0f}) {
                for (const float stick : {0.0f, 1.0f, -1.0f}) {
                    Simulation start = stagedShot(seed, y, x);
                    MatchRecording recording(start, shotStream(stick), interval);
                    if (hasHighlight(detectHighlights(recording), kind)) {
                        return {start, recording};
                    }
                }
            }
        }
    }
    FAIL("no staged shot produced the wanted highlight");
    return {Simulation(0), MatchRecording(Simulation(0), InputStream{})};
}

}

TEST_CASE("keyframes are taken every interval and the first is the start state")
{
    const Simulation start = stagedShot(1, 0.0f);
    const MatchRecording recording(start, shotStream(), 100);
    REQUIRE(recording.keyframes().size() == 5);
    REQUIRE(recording.keyframes().front().index == 0);
    REQUIRE(recording.keyframes().front().hash == start.stateHash());
    REQUIRE(recording.keyframeFor(250).index == 200);
    REQUIRE(recording.keyframeFor(kShotTicks).index == 400);
    for (const Keyframe& k : recording.keyframes()) {
        REQUIRE(k.hash == recording.hashAt(k.index));
    }
}

TEST_CASE("a goal triggers a replay window that matches the original run tick for tick")
{
    const Staged staged = findShotWith(HighlightKind::Goal, 60);
    const auto highlights = detectHighlights(staged.recording);
    const auto goal = std::find_if(highlights.begin(), highlights.end(), [](const Highlight& h) { return h.kind == HighlightKind::Goal; });
    REQUIRE(goal != highlights.end());
    REQUIRE(goal->begin <= goal->at);
    REQUIRE(goal->at <= goal->end);

    ReplayPlayer player(staged.recording, *goal);
    REQUIRE(player.index() == goal->begin);
    REQUIRE(player.stateHash() == staged.recording.hashAt(goal->begin));

    bool sawGoal = false;
    std::uint64_t played = 0;
    while (!player.done()) {
        REQUIRE(player.step());
        ++played;
        REQUIRE(player.stateHash() == staged.recording.hashAt(player.index()));
        for (const auto& e : player.world().events()) {
            sawGoal = sawGoal || e.kind == EventKind::Goal;
        }
    }
    REQUIRE(played == goal->end - goal->begin);
    REQUIRE(player.matches());
    REQUIRE(sawGoal);
    REQUIRE_FALSE(player.step());
}

TEST_CASE("any window replays the same whatever the keyframe spacing")
{
    const Staged staged = findShotWith(HighlightKind::Goal, 60);
    const MatchRecording sparse(staged.start, staged.recording.inputs(), 1000);
    for (const std::uint64_t begin : {0u, 1u, 59u, 60u, 133u, 399u}) {
        ReplayPlayer a(staged.recording, begin, 400);
        ReplayPlayer b(sparse, begin, 400);
        REQUIRE(a.stateHash() == b.stateHash());
        while (a.step()) {
            REQUIRE(b.step());
            REQUIRE(a.stateHash() == b.stateHash());
        }
        REQUIRE(a.matches());
        REQUIRE(b.matches());
    }
}

TEST_CASE("a full run replays as spectate and ends on the original final hash")
{
    const Staged staged = findShotWith(HighlightKind::Goal, 300);
    ReplayPlayer player(staged.recording, 0, staged.recording.size());
    while (player.step()) {
    }
    REQUIRE(player.matches());
    REQUIRE(player.stateHash() == staged.recording.hashAt(staged.recording.size()));
}

TEST_CASE("a staged shot that is saved is found as a save highlight")
{
    REQUIRE(hasHighlight(detectHighlights(findShotWith(HighlightKind::Save, 60).recording), HighlightKind::Save));
}

TEST_CASE("a shot that leaves over the attacked goal line is a near miss, anything else is not")
{
    const auto event = [](EventKind kind, std::uint64_t tick, std::uint32_t player, physics::Line line = physics::Line::None) {
        gameplay::MatchEvent e;
        e.kind = kind;
        e.tick = tick;
        e.player = player;
        e.line = line;
        return e;
    };
    const auto kinds = [](const std::vector<Highlight>& found) {
        std::vector<HighlightKind> result;
        for (const Highlight& h : found) {
            result.push_back(h.kind);
        }
        return result;
    };

    const std::vector<gameplay::MatchEvent> wide = {event(EventKind::Shot, 1000, 10), event(EventKind::OutOfPlay, 1040, 10, physics::Line::GoalLinePlusX)};
    const auto found = detectHighlights(wide, 2000);
    REQUIRE(kinds(found) == std::vector{HighlightKind::NearMiss});
    REQUIRE(found[0].at == 1040);
    REQUIRE(found[0].begin == 740);
    REQUIRE(found[0].end == 1220);

    const std::vector<gameplay::MatchEvent> ownEnd = {event(EventKind::Shot, 1000, 10), event(EventKind::OutOfPlay, 1040, 10, physics::Line::GoalLineMinusX)};
    REQUIRE(detectHighlights(ownEnd, 2000).empty());
    const std::vector<gameplay::MatchEvent> touchline = {event(EventKind::Shot, 1000, 10), event(EventKind::OutOfPlay, 1040, 10, physics::Line::TouchlinePlusY)};
    REQUIRE(detectHighlights(touchline, 2000).empty());
    const std::vector<gameplay::MatchEvent> late = {event(EventKind::Shot, 1000, 10), event(EventKind::OutOfPlay, 1500, 10, physics::Line::GoalLinePlusX)};
    REQUIRE(detectHighlights(late, 2000).empty());
    const std::vector<gameplay::MatchEvent> awayShot = {event(EventKind::Shot, 1000, 15), event(EventKind::OutOfPlay, 1040, 15, physics::Line::GoalLineMinusX)};
    REQUIRE(kinds(detectHighlights(awayShot, 2000)) == std::vector{HighlightKind::NearMiss});
    const std::vector<gameplay::MatchEvent> parried = {event(EventKind::Shot, 1000, 10), event(EventKind::Save, 1020, 0), event(EventKind::OutOfPlay, 1040, 10, physics::Line::GoalLinePlusX)};
    REQUIRE(kinds(detectHighlights(parried, 2000)) == std::vector{HighlightKind::Save});
}

TEST_CASE("a window past the end is clamped and a quiet run has no highlights")
{
    const Simulation start(3);
    InputStream quiet;
    for (int i = 0; i < 120; ++i) {
        quiet.push(InputFrame{});
    }
    const MatchRecording recording(start, quiet, 50);
    REQUIRE(detectHighlights(recording).empty());
    ReplayPlayer player(recording, 100, 5000);
    while (player.step()) {
    }
    REQUIRE(player.index() == 120);
    REQUIRE(player.matches());
}

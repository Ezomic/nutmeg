#include "nutmeg/ai/bot.h"
#include "nutmeg/ai/decision.h"
#include "nutmeg/core/rng.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>

using namespace nutmeg;
using namespace nutmeg::ai;
using physics::Side;
using physics::Vec2;

namespace {

constexpr std::size_t kHomeCarrier = 9;
constexpr std::size_t kHomeMateA = 8;
constexpr std::size_t kHomeMateB = 10;
constexpr std::size_t kAwayCarrier = gameplay::kPlayersPerSide + 9;

// Moves every player but the named ones out to the corners, so the scenario
// holds only the players it mentions.
void clearField(gameplay::World& world, std::initializer_list<std::size_t> keep)
{
    for (std::size_t i = 0; i < gameplay::kPlayerCount; ++i) {
        if (std::find(keep.begin(), keep.end(), i) != keep.end()) {
            continue;
        }
        const float x = i < gameplay::kPlayersPerSide ? -50.0f : 50.0f;
        world.players()[i].position = {x, -33.0f + static_cast<float>(i % gameplay::kPlayersPerSide) * 0.5f};
    }
}

void put(gameplay::World& world, std::size_t index, Vec2 at)
{
    world.players()[index].position = at;
}

const Candidate* find(const Decision& d, CandidateKind kind)
{
    const auto it = std::find_if(d.candidates.begin(), d.candidates.end(), [&](const Candidate& c) { return c.kind == kind; });
    return it == d.candidates.end() ? nullptr : &*it;
}

struct Totals {
    int decisions = 0;
    std::array<int, kCandidateKinds> chosen{};
    double passChance = 0.0;
    int passes = 0;
    double shotChance = 0.0;
    int shots = 0;
};

// Lines both teams up in their planned shape around a random ball spot, puts the
// nearest attacker on the ball, and records what he chooses.
Totals sampleDecisions(std::uint64_t seed, int samples)
{
    Rng rng(seed);
    Totals totals;
    for (int n = 0; n < samples; ++n) {
        gameplay::World world(seed + static_cast<std::uint64_t>(n));
        const Vec2 ball{(rng.nextFloat() - 0.5f) * 90.0f, (rng.nextFloat() - 0.5f) * 60.0f};

        for (const Side side : {Side::Home, Side::Away}) {
            TeamInput in;
            in.side = side;
            in.formation = Formation::F442;
            in.control = side == Side::Home ? BallControl::Home : BallControl::Away;
            in.ball = ball;
            for (std::size_t i = 0; i < kOutfielders; ++i) {
                in.positions[i] = world.players()[(side == Side::Home ? 0 : gameplay::kPlayersPerSide) + 1 + i].position;
                in.opponents[i] = in.positions[i];
            }
            const TeamIntent intent = planTeam(in);
            for (std::size_t i = 0; i < kOutfielders; ++i) {
                const Vec2 jitter{(rng.nextFloat() - 0.5f) * 8.0f, (rng.nextFloat() - 0.5f) * 8.0f};
                world.players()[(side == Side::Home ? 0 : gameplay::kPlayersPerSide) + 1 + i].position = intent.targets[i].point + jitter;
            }
        }

        std::size_t carrier = 1;
        for (std::size_t i = 1; i < gameplay::kPlayersPerSide; ++i) {
            if (physics::distance(world.players()[i].position, ball) < physics::distance(world.players()[carrier].position, ball)) {
                carrier = i;
            }
        }
        world.players()[carrier].position = ball;

        const Decision d = evaluate(world, carrier);
        const Candidate& best = d.best();
        ++totals.decisions;
        ++totals.chosen[static_cast<std::size_t>(best.kind)];
        if (best.kind == CandidateKind::Pass || best.kind == CandidateKind::Switch || best.kind == CandidateKind::Through) {
            totals.passChance += best.chance;
            ++totals.passes;
        } else if (best.kind == CandidateKind::Shoot) {
            totals.shotChance += best.chance;
            ++totals.shots;
        }
    }
    return totals;
}

}

TEST_CASE("an open player in range shoots")
{
    gameplay::World world;
    clearField(world, {kHomeCarrier, kHomeMateA, kHomeMateB});
    put(world, kHomeCarrier, {40.0f, 2.0f});
    put(world, kHomeMateA, {30.0f, 12.0f});
    put(world, kHomeMateB, {32.0f, -10.0f});

    const Decision d = evaluate(world, kHomeCarrier);
    REQUIRE(d.best().kind == CandidateKind::Shoot);
    REQUIRE(d.best().request.kind == gameplay::ActionKind::Shoot);
    REQUIRE(d.best().request.aim.x == 52.5f);
}

TEST_CASE("nobody shoots from their own half")
{
    gameplay::World world;
    clearField(world, {kHomeCarrier, kHomeMateA});
    put(world, kHomeCarrier, {-25.0f, 0.0f});
    put(world, kHomeMateA, {-10.0f, 8.0f});

    const Decision d = evaluate(world, kHomeCarrier);
    REQUIRE(find(d, CandidateKind::Shoot) == nullptr);
    REQUIRE(d.best().kind != CandidateKind::Shoot);
}

TEST_CASE("a pressed carrier passes to a free teammate instead of holding")
{
    gameplay::World world;
    clearField(world, {kHomeCarrier, kHomeMateA, kAwayCarrier});
    put(world, kHomeCarrier, {-20.0f, 0.0f});
    put(world, kHomeMateA, {-8.0f, 10.0f});
    put(world, kAwayCarrier, {-21.0f, 0.5f});

    const Decision d = evaluate(world, kHomeCarrier);
    REQUIRE(d.best().request.kind == gameplay::ActionKind::Pass);
    REQUIRE(d.best().request.target == kHomeMateA);
}

TEST_CASE("a covered passing lane is worth less than an open one")
{
    gameplay::World world;
    clearField(world, {kHomeCarrier, kHomeMateA, kHomeMateB, kAwayCarrier});
    put(world, kHomeCarrier, {0.0f, 0.0f});
    put(world, kHomeMateA, {15.0f, 10.0f});
    put(world, kHomeMateB, {15.0f, -10.0f});
    put(world, kAwayCarrier, {7.5f, 5.0f});

    const Decision d = evaluate(world, kHomeCarrier);
    float covered = -1.0f;
    float open = -1.0f;
    for (const Candidate& c : d.candidates) {
        if (c.kind == CandidateKind::Pass && c.request.target == kHomeMateA) {
            covered = c.chance;
        }
        if (c.kind == CandidateKind::Pass && c.request.target == kHomeMateB) {
            open = c.chance;
        }
    }
    REQUIRE(covered >= 0.0f);
    REQUIRE(open > covered + 0.2f);
}

TEST_CASE("a wide carrier near the byline can cross to a teammate in the box")
{
    gameplay::World world;
    clearField(world, {kHomeCarrier, kHomeMateA});
    put(world, kHomeCarrier, {40.0f, 28.0f});
    put(world, kHomeMateA, {46.0f, 2.0f});

    const Decision d = evaluate(world, kHomeCarrier);
    const Candidate* cross = find(d, CandidateKind::Cross);
    REQUIRE(cross != nullptr);
    REQUIRE(cross->request.kind == gameplay::ActionKind::Cross);
    REQUIRE(cross->request.target == kHomeMateA);
}

TEST_CASE("a pass to the far flank is a switch and a pass in behind is a through ball")
{
    gameplay::World world;
    clearField(world, {kHomeCarrier, kHomeMateA, kHomeMateB});
    put(world, kHomeCarrier, {0.0f, 15.0f});
    put(world, kHomeMateA, {2.0f, -15.0f});
    put(world, kHomeMateB, {25.0f, 12.0f});

    const Decision d = evaluate(world, kHomeCarrier);
    const Candidate* sw = find(d, CandidateKind::Switch);
    const Candidate* through = find(d, CandidateKind::Through);
    REQUIRE(sw != nullptr);
    REQUIRE(sw->request.target == kHomeMateA);
    REQUIRE(through != nullptr);
    REQUIRE(through->request.kind == gameplay::ActionKind::Through);
    REQUIRE(through->request.target == kHomeMateB);
}

TEST_CASE("the away side decides as the mirror of the home side")
{
    gameplay::World home;
    gameplay::World away;
    clearField(home, {kHomeCarrier, kHomeMateA, kAwayCarrier});
    clearField(away, {kAwayCarrier, kAwayCarrier - 1, kHomeCarrier});
    put(home, kHomeCarrier, {30.0f, 8.0f});
    put(home, kHomeMateA, {35.0f, -6.0f});
    put(home, kAwayCarrier, {36.0f, 9.0f});
    put(away, kAwayCarrier, {-30.0f, -8.0f});
    put(away, kAwayCarrier - 1, {-35.0f, 6.0f});
    put(away, kHomeCarrier, {-36.0f, -9.0f});

    const Decision a = evaluate(home, kHomeCarrier);
    const Decision b = evaluate(away, kAwayCarrier);
    REQUIRE(a.candidates.size() == b.candidates.size());
    REQUIRE(a.best().kind == b.best().kind);
    REQUIRE(std::abs(a.best().utility - b.best().utility) < 1e-4f);
}

TEST_CASE("a decision lists every scored candidate and picks the highest utility")
{
    gameplay::World world;
    clearField(world, {kHomeCarrier, kHomeMateA, kHomeMateB});
    put(world, kHomeCarrier, {30.0f, 5.0f});
    put(world, kHomeMateA, {28.0f, 14.0f});
    put(world, kHomeMateB, {38.0f, -6.0f});

    const Decision d = evaluate(world, kHomeCarrier);
    REQUIRE(d.player == kHomeCarrier);
    REQUIRE(find(d, CandidateKind::Hold) != nullptr);
    REQUIRE(find(d, CandidateKind::Dribble) != nullptr);
    for (const Candidate& c : d.candidates) {
        REQUIRE(c.utility == c.gain - c.risk);
        REQUIRE(c.chance >= 0.0f);
        REQUIRE(c.chance <= 1.0f);
        REQUIRE(c.utility <= d.best().utility);
    }
}

TEST_CASE("the same situation scores identically every time")
{
    gameplay::World world;
    clearField(world, {kHomeCarrier, kHomeMateA, kHomeMateB, kAwayCarrier});
    put(world, kHomeCarrier, {20.0f, 5.0f});
    put(world, kHomeMateA, {28.0f, 14.0f});
    put(world, kHomeMateB, {26.0f, -6.0f});
    put(world, kAwayCarrier, {24.0f, 3.0f});

    const Decision a = evaluate(world, kHomeCarrier);
    const Decision b = evaluate(world, kHomeCarrier);
    REQUIRE(a.chosen == b.chosen);
    REQUIRE(a.candidates.size() == b.candidates.size());
    for (std::size_t i = 0; i < a.candidates.size(); ++i) {
        REQUIRE(a.candidates[i].utility == b.candidates[i].utility);
        REQUIRE(a.candidates[i].request == b.candidates[i].request);
    }
}

TEST_CASE("better attributes make a shot likelier")
{
    gameplay::World world;
    clearField(world, {kHomeCarrier});
    put(world, kHomeCarrier, {42.0f, 4.0f});
    const float base = shotChance(world, world.players()[kHomeCarrier]);
    world.players()[kHomeCarrier].profile.attributes.finishing = 95;
    REQUIRE(shotChance(world, world.players()[kHomeCarrier]) > base);
}

TEST_CASE("shot chance falls with distance and with a defender in the way")
{
    gameplay::World world;
    clearField(world, {kHomeCarrier, kAwayCarrier});
    put(world, kHomeCarrier, {44.0f, 0.0f});
    const float near = shotChance(world, world.players()[kHomeCarrier]);
    put(world, kHomeCarrier, {30.0f, 0.0f});
    const float far = shotChance(world, world.players()[kHomeCarrier]);
    REQUIRE(near > far);

    put(world, kAwayCarrier, {41.0f, 0.0f});
    put(world, kHomeCarrier, {35.0f, 0.0f});
    const float blocked = shotChance(world, world.players()[kHomeCarrier]);
    put(world, kAwayCarrier, {41.0f, 15.0f});
    REQUIRE(shotChance(world, world.players()[kHomeCarrier]) > blocked);
}

namespace {

struct MatchRun {
    std::uint64_t hash = 0;
    DecisionStats stats;
    std::size_t logged = 0;
    int kicks = 0;
};

MatchRun playMatch(int ticks, std::uint64_t seed)
{
    gameplay::World world(seed);
    Decider homeDecider;
    Decider awayDecider;
    MatchRun run;
    for (int t = 0; t < ticks; ++t) {
        gameplay::PlayerInputs inputs{};
        gameplay::PlayerActions actions{};
        const std::uint32_t holder = world.possessor();
        const auto control = [&]() {
            if (holder == gameplay::kNoPlayer) {
                return BallControl::Loose;
            }
            const bool homeHolds = world.players()[holder].side == Side::Home;
            return homeHolds ? BallControl::Home : BallControl::Away;
        };
        drive(world, Side::Home, Formation::F442, Mentality::Balanced, control(), inputs, actions, homeDecider);
        drive(world, Side::Away, Formation::F442, Mentality::Balanced, control(), inputs, actions, awayDecider);
        world.step(inputs, actions);
        for (const gameplay::MatchEvent& e : world.events()) {
            run.kicks += e.kind == gameplay::EventKind::Pass || e.kind == gameplay::EventKind::Shot;
        }
    }
    run.hash = world.hash();
    run.stats = homeDecider.stats();
    run.logged = homeDecider.log().size() + awayDecider.log().size();
    return run;
}

}

TEST_CASE("two AI sides playing the same seed reach the same world")
{
    const MatchRun a = playMatch(60 * 20, 7);
    const MatchRun b = playMatch(60 * 20, 7);
    REQUIRE(a.hash == b.hash);
    REQUIRE(a.logged > 0);
}

TEST_CASE("the AI chases a loose ball, takes decisions and kicks it")
{
    const MatchRun run = playMatch(60 * 30, 1);
    REQUIRE(run.logged > 0);
    REQUIRE(run.kicks > 0);
}

TEST_CASE("the decision log is bounded and the tally counts every decision")
{
    gameplay::World world(1);
    Decider decider;
    for (int t = 0; t < 60 * 60; ++t) {
        gameplay::PlayerInputs inputs{};
        gameplay::PlayerActions actions{};
        drive(world, Side::Home, Formation::F442, Mentality::Balanced, BallControl::Loose, inputs, actions, decider);
        world.step(inputs, actions);
    }
    const std::uint32_t decisions = decider.stats().decisions;
    std::uint32_t sum = 0;
    for (const std::uint32_t n : decider.stats().chosen) {
        sum += n;
    }
    REQUIRE(sum == decisions);
    REQUIRE(decider.log().size() <= Decider::kLogCapacity);
    REQUIRE(decider.log().size() == std::min<std::size_t>(decisions, Decider::kLogCapacity));
}

TEST_CASE("sampled decisions sit inside the realism bands")
{
    const Totals t = sampleDecisions(11, 1500);
    REQUIRE(t.passes > 0);
    REQUIRE(t.shots > 0);

    // data/realism_bands.json: pass_completion_pct 70 to 88, shot_conversion_pct 9 to 14.
    const double expectedCompletion = t.passChance / t.passes;
    const double expectedConversion = t.shotChance / t.shots;
    INFO("completion " << expectedCompletion << " conversion " << expectedConversion << " passes " << t.passes << " shots " << t.shots << " dribbles " << t.chosen[1] << " holds " << t.chosen[6] << " crosses " << t.chosen[4] << " of " << t.decisions);
    REQUIRE(expectedCompletion > 0.70);
    REQUIRE(expectedCompletion < 0.88);
    REQUIRE(expectedConversion > 0.06);
    REQUIRE(expectedConversion < 0.20);

    // Passing is the bread and butter; shooting is rare; all but a sliver of decisions are movement or kicks.
    const double passShare = static_cast<double>(t.passes) / t.decisions;
    const double shotShare = static_cast<double>(t.shots) / t.decisions;
    INFO("pass share " << passShare << " shot share " << shotShare);
    REQUIRE(passShare > 0.3);
    REQUIRE(shotShare < 0.15);
    REQUIRE(t.chosen[static_cast<std::size_t>(CandidateKind::Cross)] > 0);
}

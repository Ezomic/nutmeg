#include "nutmeg/ai/tactics.h"

#include <catch2/catch_test_macros.hpp>

#include <array>

using namespace nutmeg;
using namespace nutmeg::ai;
using physics::Side;

namespace {

constexpr int kTicks = 60 * 90;
constexpr int kSampleEvery = 30;
constexpr std::array<std::uint64_t, 16> kSeeds{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
constexpr std::array<Mentality, 3> kMentalities{Mentality::Attacking, Mentality::Balanced, Mentality::Defensive};

Tactics with(Mentality m, Difficulty d = Difficulty::Normal) { return Tactics{Formation::F442, m, d}; }

std::size_t slot(Mentality m)
{
    return static_cast<std::size_t>(m);
}

BallControl controlOf(const gameplay::World& world)
{
    const std::uint32_t holder = world.possessor();
    if (holder == gameplay::kNoPlayer) {
        return BallControl::Loose;
    }
    return world.players()[holder].side == Side::Home ? BallControl::Home : BallControl::Away;
}

std::array<physics::Vec2, kOutfielders> targetsOf(const TeamIntent& intent)
{
    std::array<physics::Vec2, kOutfielders> out{};
    for (std::size_t i = 0; i < kOutfielders; ++i) {
        out[i] = intent.targets[i].point;
    }
    return out;
}

struct Batch {
    // Shape the home plan gives under each mentality, from the same world states.
    std::array<ShapeMetrics, 3> shape{};
    // What the home carrier would choose under each mentality, from the same world states.
    std::array<double, 3> directShare{};
    std::array<double, 3> chosenChance{};
    std::array<int, 3> decisions{};
    int samples = 0;
    std::uint64_t hash = 0;
    std::uint32_t homeDecisions = 0;
};

bool isDirect(CandidateKind kind)
{
    return kind == CandidateKind::Shoot || kind == CandidateKind::Through || kind == CandidateKind::Cross;
}

Batch playBatch(const Tactics& home, const Tactics& away)
{
    Batch batch;
    for (const std::uint64_t seed : kSeeds) {
        gameplay::World world(seed);
        Decider homeDecider = makeDecider(home, seed);
        Decider awayDecider = makeDecider(away, seed + 100);
        for (int t = 0; t < kTicks; ++t) {
            gameplay::PlayerInputs inputs{};
            gameplay::PlayerActions actions{};
            const BallControl control = controlOf(world);

            if (t % kSampleEvery == 0) {
                for (const Mentality m : kMentalities) {
                    Tactics probe = home;
                    probe.mentality = m;
                    const TeamIntent intent = planTeam(inputFor(world, Side::Home, probe.formation, m, control));
                    const ShapeMetrics s = measure(Side::Home, probe.formation, targetsOf(intent));
                    batch.shape[slot(m)].lineHeight += s.lineHeight;
                    batch.shape[slot(m)].blockLength += s.blockLength;
                    batch.shape[slot(m)].blockWidth += s.blockWidth;
                }
                ++batch.samples;
            }
            const std::uint32_t holder = world.possessor();
            if (control == BallControl::Home && t % 10 == 0 && world.players()[holder].cooldown == 0) {
                for (const Mentality m : kMentalities) {
                    const Decision d = evaluate(world, holder, styleFor(home.difficulty, m).decision.riskAversion);
                    ++batch.decisions[slot(m)];
                    batch.directShare[slot(m)] += isDirect(d.best().kind) ? 1.0 : 0.0;
                    batch.chosenChance[slot(m)] += d.best().chance;
                }
            }

            drive(world, Side::Home, home, control, inputs, actions, homeDecider);
            drive(world, Side::Away, away, control, inputs, actions, awayDecider);
            world.step(inputs, actions);
        }
        batch.hash ^= world.hash();
        batch.homeDecisions += homeDecider.stats().decisions;
    }
    for (const Mentality m : kMentalities) {
        ShapeMetrics& s = batch.shape[slot(m)];
        s.lineHeight /= static_cast<float>(batch.samples);
        s.blockLength /= static_cast<float>(batch.samples);
        s.blockWidth /= static_cast<float>(batch.samples);
        if (batch.decisions[slot(m)] > 0) {
            batch.directShare[slot(m)] /= batch.decisions[slot(m)];
            batch.chosenChance[slot(m)] /= batch.decisions[slot(m)];
        }
    }
    return batch;
}

}

TEST_CASE("the runner's strings map onto tactics")
{
    const auto t = parseTactics("433", "attacking", "hard");
    REQUIRE(t);
    REQUIRE(t->formation == Formation::F433);
    REQUIRE(t->mentality == Mentality::Attacking);
    REQUIRE(t->difficulty == Difficulty::Hard);
    REQUIRE(parseTactics("442", "balanced")->difficulty == Difficulty::Normal);
    REQUIRE_FALSE(parseTactics("999", "balanced"));
    REQUIRE_FALSE(parseTactics("442", "reckless"));
    REQUIRE_FALSE(parseTactics("442", "balanced", "legendary"));
}

TEST_CASE("mentality and difficulty resolve to ordered AI parameters")
{
    const Style attacking = styleFor(Difficulty::Normal, Mentality::Attacking);
    const Style balanced = styleFor(Difficulty::Normal, Mentality::Balanced);
    const Style defensive = styleFor(Difficulty::Normal, Mentality::Defensive);
    REQUIRE(attacking.decision.riskAversion < balanced.decision.riskAversion);
    REQUIRE(balanced.decision.riskAversion < defensive.decision.riskAversion);
    REQUIRE(attacking.press > balanced.press);
    REQUIRE(balanced.press > defensive.press);

    const Style easy = styleFor(Difficulty::Easy, Mentality::Balanced);
    const Style hard = styleFor(Difficulty::Hard, Mentality::Balanced);
    REQUIRE(easy.decision.reactionTicks > hard.decision.reactionTicks);
    REQUIRE(easy.decision.noise > hard.decision.noise);
    REQUIRE(easy.press < hard.press);
}

TEST_CASE("the tactics drive is deterministic for a seed")
{
    const Batch a = playBatch(with(Mentality::Attacking, Difficulty::Easy), with(Mentality::Defensive));
    const Batch b = playBatch(with(Mentality::Attacking, Difficulty::Easy), with(Mentality::Defensive));
    REQUIRE(a.hash == b.hash);
    REQUIRE(a.chosenChance == b.chosenChance);
}

TEST_CASE("in seeded batches, mentality changes shape and risk")
{
    const Batch batch = playBatch(with(Mentality::Balanced), with(Mentality::Balanced));
    const auto a = slot(Mentality::Attacking);
    const auto b = slot(Mentality::Balanced);
    const auto d = slot(Mentality::Defensive);
    INFO("line " << batch.shape[a].lineHeight << " " << batch.shape[b].lineHeight << " " << batch.shape[d].lineHeight);
    INFO("length " << batch.shape[a].blockLength << " " << batch.shape[b].blockLength << " " << batch.shape[d].blockLength);
    INFO("width " << batch.shape[a].blockWidth << " " << batch.shape[b].blockWidth << " " << batch.shape[d].blockWidth);
    INFO("direct " << batch.directShare[a] << " " << batch.directShare[b] << " " << batch.directShare[d]);
    INFO("chance " << batch.chosenChance[a] << " " << batch.chosenChance[b] << " " << batch.chosenChance[d]);
    INFO("decisions " << batch.decisions[a]);

    REQUIRE(batch.decisions[a] > 100);
    REQUIRE(batch.shape[a].lineHeight > batch.shape[b].lineHeight + 1.0f);
    REQUIRE(batch.shape[b].lineHeight > batch.shape[d].lineHeight + 1.0f);
    REQUIRE(batch.shape[a].blockWidth > batch.shape[d].blockWidth);

    REQUIRE(batch.directShare[a] > batch.directShare[b]);
    REQUIRE(batch.directShare[b] > batch.directShare[d]);
    REQUIRE(batch.chosenChance[a] < batch.chosenChance[b]);
    REQUIRE(batch.chosenChance[b] < batch.chosenChance[d]);
}

TEST_CASE("an easy side decides later than a hard one")
{
    const Batch easy = playBatch(with(Mentality::Balanced, Difficulty::Easy), with(Mentality::Balanced));
    const Batch hard = playBatch(with(Mentality::Balanced, Difficulty::Hard), with(Mentality::Balanced));
    INFO("decisions " << easy.homeDecisions << " " << hard.homeDecisions);
    REQUIRE(easy.homeDecisions < hard.homeDecisions);
}

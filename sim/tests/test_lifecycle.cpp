#include "nutmeg/sim/simulation.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

using namespace nutmeg;
using gameplay::LifecycleEvent;
using gameplay::LifecycleEventKind;
using gameplay::Phase;
using gameplay::Team;

namespace {

constexpr int kTicksPerMinute = 60;

gameplay::LifecycleConfig fastMatch()
{
    gameplay::LifecycleConfig config;
    config.ticksPerMinute = kTicksPerMinute;
    config.injuryPerPlayerMinute = 0.05f;
    config.substitutionFromMinute = 40;
    config.intervalSubstitutionChance = 1.0f;
    return config;
}

InputFrame scripted(std::uint64_t tick)
{
    InputFrame frame;
    frame.moveX = static_cast<float>(tick % 7) * 0.25f - 0.75f;
    frame.moveY = static_cast<float>(tick % 5) * 0.5f - 1.0f;
    frame.buttons = tick % 11 == 0 ? InputFrame::Pass : (tick % 13 == 0 ? InputFrame::Shoot : 0u);
    return frame;
}

std::uint64_t playToFullTime(Simulation& sim, std::uint64_t maxTicks)
{
    std::uint64_t tick = 0;
    while (sim.lifecycle().phase() != Phase::FullTime && tick < maxTicks) {
        sim.tick(scripted(tick));
        ++tick;
    }
    return tick;
}

std::vector<LifecycleEvent> eventsOf(const Simulation& sim, LifecycleEventKind kind)
{
    std::vector<LifecycleEvent> found;
    for (const LifecycleEvent& event : sim.lifecycle().log()) {
        if (event.kind == kind) {
            found.push_back(event);
        }
    }
    return found;
}

}

TEST_CASE("a full 90 minute match runs headless through its phases", "[lifecycle]")
{
    Simulation sim(7, fastMatch());
    const std::uint64_t ticks = playToFullTime(sim, 200000);
    REQUIRE(sim.lifecycle().phase() == Phase::FullTime);

    const auto phases = eventsOf(sim, LifecycleEventKind::PhaseChange);
    REQUIRE(phases.size() == 4);
    CHECK(phases[0].phase == Phase::FirstHalf);
    CHECK(phases[1].phase == Phase::HalfTime);
    CHECK(phases[2].phase == Phase::SecondHalf);
    CHECK(phases[3].phase == Phase::FullTime);

    const std::uint64_t half = 45 * kTicksPerMinute;
    const std::uint64_t interval = 15 * kTicksPerMinute;
    CHECK(phases[0].tick == 0);
    CHECK(phases[1].tick >= half);
    CHECK(phases[2].tick - phases[1].tick >= interval - 1);
    CHECK(phases[2].tick - phases[1].tick <= interval + 1);
    CHECK(phases[3].tick - phases[2].tick >= half);
    CHECK(ticks >= phases[3].tick);

    const auto added = eventsOf(sim, LifecycleEventKind::AddedTime);
    REQUIRE(added.size() == 2);
    CHECK(added[0].phase == Phase::FirstHalf);
    CHECK(added[1].phase == Phase::SecondHalf);
    for (const LifecycleEvent& event : added) {
        CHECK(event.value >= 0.0f);
        CHECK(event.value <= 6.0f);
    }
    CHECK(phases[1].tick >= half + static_cast<std::uint64_t>(added[0].value) * kTicksPerMinute);
    CHECK(phases[3].tick - phases[2].tick >= half + static_cast<std::uint64_t>(added[1].value) * kTicksPerMinute);

    CHECK(sim.lifecycle().endsSwapped());
    CHECK(sim.lifecycle().sideOf(Team::A) == physics::Side::Away);

    CHECK(!eventsOf(sim, LifecycleEventKind::Injury).empty());
    const auto subs = eventsOf(sim, LifecycleEventKind::Substitution);
    CHECK(!subs.empty());
    CHECK(sim.lifecycle().substitutionsMade(Team::A) <= 5);
    CHECK(sim.lifecycle().substitutionsMade(Team::B) <= 5);
    CHECK(sim.lifecycle().substitutionsMade(Team::A) + sim.lifecycle().substitutionsMade(Team::B) == static_cast<int>(subs.size()));
}

TEST_CASE("the second half kicks off for the other team after the ends swap", "[lifecycle]")
{
    Simulation sim(11, fastMatch());
    std::uint64_t tick = 0;
    while (sim.lifecycle().phase() != Phase::SecondHalf && tick < 200000) {
        sim.tick(scripted(tick));
        ++tick;
    }
    REQUIRE(sim.lifecycle().phase() == Phase::SecondHalf);
    REQUIRE(sim.world().restart().has_value());
    CHECK(sim.world().restart()->kind == gameplay::RestartKind::Kickoff);
    CHECK(sim.world().restart()->side == sim.lifecycle().sideOf(Team::B));
    CHECK(sim.world().restart()->side == physics::Side::Home);
}

TEST_CASE("the world stands still during the interval", "[lifecycle]")
{
    Simulation sim(3, fastMatch());
    std::uint64_t tick = 0;
    while (sim.lifecycle().phase() != Phase::HalfTime && tick < 200000) {
        sim.tick(scripted(tick));
        ++tick;
    }
    REQUIRE(sim.lifecycle().phase() == Phase::HalfTime);
    const std::uint64_t worldTick = sim.world().tick();
    for (int i = 0; i < 100; ++i) {
        sim.tick(scripted(tick++));
    }
    CHECK(sim.world().tick() == worldTick);
}

TEST_CASE("the same seed plays the same match", "[lifecycle]")
{
    Simulation a(5, fastMatch());
    Simulation b(5, fastMatch());
    playToFullTime(a, 200000);
    playToFullTime(b, 200000);
    CHECK(a.stateHash() == b.stateHash());
    CHECK(a.lifecycle().log() == b.lifecycle().log());
}

TEST_CASE("substitutions are capped and keepers are replaced by keepers", "[lifecycle]")
{
    gameplay::World world(1);
    gameplay::Lifecycle lifecycle(Rng(1).derive("lifecycle"), fastMatch());

    CHECK(lifecycle.substitute(world, 0));
    CHECK(world.players()[0].profile.position == schema::Position::Goalkeeper);
    CHECK(world.players()[0].profile.id >= gameplay::kPlayerCount);
    CHECK(!lifecycle.substitute(world, 0));

    for (std::uint32_t slot = 1; slot <= 4; ++slot) {
        CHECK(lifecycle.substitute(world, slot));
    }
    CHECK(lifecycle.substitutionsMade(Team::A) == 5);
    CHECK(!lifecycle.substitute(world, 5));
    CHECK(lifecycle.substitute(world, 11 + 1));
    CHECK(lifecycle.substitutionsMade(Team::B) == 1);
}

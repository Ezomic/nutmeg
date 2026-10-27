#pragma once

#include "nutmeg/core/rng.h"
#include "nutmeg/gameplay/world.h"
#include "nutmeg/schema/player.h"

#include <array>
#include <cstdint>
#include <vector>

namespace nutmeg::gameplay {

enum class Phase : std::uint8_t { PreMatch, FirstHalf, HalfTime, SecondHalf, FullTime };

// The two sides of the fixture. Team A kicks off the match and starts as Home;
// the teams change ends at the interval, so their physical `Side` swaps.
enum class Team : std::uint8_t { A, B };

enum class LifecycleEventKind : std::uint8_t {
    PhaseChange,  // phase: the phase that has just begun
    AddedTime,    // phase: the half it applies to; value: whole minutes added
    Injury,       // player: the slot of the injured player; side: their physical side
    Substitution, // player: the slot; side: its physical side; value: profile id of the player who came on
    PlayerLeft,   // player: the slot of an injured player nobody could replace; side: its physical side
};

struct LifecycleEvent {
    LifecycleEventKind kind = LifecycleEventKind::PhaseChange;
    std::uint64_t tick = 0;
    Phase phase = Phase::PreMatch;
    physics::Side side = physics::Side::Home;
    std::uint32_t player = kNoPlayer;
    float value = 0.0f;

    bool operator==(const LifecycleEvent&) const = default;
};

// Tuning for the match clock, stoppages, injuries and substitutions.
struct LifecycleConfig {
    int ticksPerMinute = 3600; // a real-time match; shrink it to play a whole match quickly
    int halfMinutes = 45;
    int intervalMinutes = 15;
    int maxAddedMinutes = 6;
    // Share of the ticks the ball was dead that is played back as added time.
    float stoppageCredit = 0.5f;
    // Extra added time for each substitution, in minutes.
    float stoppageMinutesPerChange = 0.5f;

    float foulInjuryChance = 0.03f;
    float injuryPerPlayerMinute = 0.0005f;

    int maxSubstitutions = 5;
    // A tactical change may be made at a stoppage or the interval from this minute on.
    int substitutionFromMinute = 55;
    float stoppageSubstitutionChance = 0.1f;
    float intervalSubstitutionChance = 0.5f;
    int benchSize = 7;

    // A restart nobody takes is taken for them after this many ticks.
    int restartDelayTicks = 60;
};

// Runs the match around a World: the clock, the two halves and the interval
// (with the teams changing ends), added time, injuries and substitutions.
// Call before() ahead of each World::step and after() once it has run. While
// the match is not playing (the interval and full time) the World must not be
// stepped.
class Lifecycle {
public:
    explicit Lifecycle(Rng rng, LifecycleConfig config = {});

    // Handles the start of the match and the interval, and applies injuries
    // and substitutions at a stoppage. Returns the actions that take any
    // restart that has waited too long.
    PlayerActions before(World& world);
    // Reads the step's events: advances the clock, counts goals, rolls injuries
    // and ends the half when its time is up.
    void after(World& world);

    // Replaces the player in `slot` with the next suitable one from the bench.
    bool substitute(World& world, std::uint32_t slot);

    Phase phase() const { return phase_; }
    bool playing() const { return phase_ == Phase::FirstHalf || phase_ == Phase::SecondHalf; }
    // Match minute, not counting added time past the end of the half.
    int minute() const;
    int addedMinutes() const { return addedMinutes_; }
    bool endsSwapped() const { return swapped_; }
    physics::Side sideOf(Team team) const;
    int goals(Team team) const { return goals_[static_cast<std::size_t>(team)]; }
    int substitutionsMade(Team team) const { return used_[static_cast<std::size_t>(team)]; }
    // Everything that has happened, in order.
    const std::vector<LifecycleEvent>& log() const { return log_; }
    std::uint64_t hash() const;

private:
    Team teamOf(physics::Side side) const;
    void setPhase(Phase phase);
    void emit(LifecycleEvent event);
    void startHalf(World& world, Team kickingOff);
    void swapEnds(World& world);
    void tallyStoppage(const World& world);
    void rollInjuries(World& world);
    void injure(const World& world, std::uint32_t slot);
    void resolveInjuries(World& world);
    void tryTactical(World& world, float chance);
    void endHalf(World& world);
    void leavePitch(Player& player);
    PlayerActions takeRestart(const World& world) const;

    Rng rng_;
    LifecycleConfig config_;
    Phase phase_ = Phase::PreMatch;
    std::uint64_t tick_ = 0;
    std::uint64_t halfTicks_ = 0;
    std::uint64_t stoppedTicks_ = 0;
    std::uint64_t creditTicks_ = 0;
    std::uint64_t intervalLeft_ = 0;
    std::uint64_t handledRestart_ = ~0ull;
    int addedMinutes_ = 0;
    bool addedDecided_ = false;
    bool swapped_ = false;
    std::array<int, 2> goals_{};
    std::array<int, 2> used_{};
    std::array<std::vector<schema::PlayerProfile>, 2> bench_;
    std::vector<std::uint32_t> injured_;
    std::vector<LifecycleEvent> log_;
};

}

#pragma once

#include "nutmeg/gameplay/actions.h"
#include "nutmeg/gameplay/world.h"
#include "nutmeg/physics/pitch.h"
#include "nutmeg/physics/vec.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <string_view>
#include <vector>

namespace nutmeg::ai {

enum class CandidateKind : std::uint8_t { Shoot, Dribble, Pass, Through, Cross, Switch, Hold };

inline constexpr std::size_t kCandidateKinds = 7;

std::string_view candidateName(CandidateKind kind);

// One thing the ball carrier could do, scored. utility = gain - risk: `gain`
// is the chance it works times the threat it adds, `risk` the chance it fails
// times what losing the ball there costs.
struct Candidate {
    CandidateKind kind = CandidateKind::Hold;
    gameplay::ActionRequest request;
    // Where a dribble heads. Equal to the carrier for a hold.
    physics::Vec2 destination;
    float chance = 0.0f;
    float gain = 0.0f;
    float risk = 0.0f;
    float utility = 0.0f;
};

// Every candidate considered at one decision, in the order they were scored,
// and which one won (first wins a tie).
struct Decision {
    std::uint64_t tick = 0;
    std::uint32_t player = gameplay::kNoPlayer;
    std::vector<Candidate> candidates;
    std::size_t chosen = 0;

    const Candidate& best() const { return candidates[chosen]; }
};

// Scores every option for the carrier at index `carrier` and picks the best.
// A pure function of the world: no randomness, no state.
Decision evaluate(const gameplay::World& world, std::size_t carrier);

// Threat of standing with the ball at `point` for a team attacking `side`'s goal.
float threatAt(physics::Side attacking, physics::Vec2 point);

// Chance a shot from where `shooter` stands scores, before the keeper's quality is known.
float shotChance(const gameplay::World& world, const gameplay::Player& shooter);

// The outfielder of `side` nearest a ball nobody holds, or kNoPlayer when the
// ball is held or dead. Nearest wins, lowest index on a tie.
std::uint32_t looseBallChaser(const gameplay::World& world, physics::Side side);

struct CarrierPlan {
    std::uint32_t player = gameplay::kNoPlayer;
    // Set when the carrier kicks this tick.
    gameplay::ActionRequest request;
    // Set when the carrier should steer instead of following the team shape.
    bool steer = false;
    physics::Vec2 destination;
};

struct DecisionStats {
    std::array<std::uint32_t, kCandidateKinds> chosen{};
    std::uint32_t decisions = 0;
};

// Gives one side's ball carrier a decision every few ticks and keeps a bounded
// record of recent decisions plus a running tally.
class Decider {
public:
    static constexpr std::size_t kLogCapacity = 256;

    CarrierPlan plan(const gameplay::World& world, physics::Side side);

    const std::deque<Decision>& log() const { return log_; }
    const DecisionStats& stats() const { return stats_; }

private:
    std::uint32_t carrier_ = gameplay::kNoPlayer;
    std::uint64_t nextTick_ = 0;
    bool steering_ = false;
    physics::Vec2 destination_;
    std::deque<Decision> log_;
    DecisionStats stats_;
};

}

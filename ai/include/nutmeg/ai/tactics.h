#pragma once

#include "nutmeg/ai/bot.h"
#include "nutmeg/ai/decision.h"
#include "nutmeg/ai/formation.h"
#include "nutmeg/ai/tactics_tuning.h"

#include <optional>
#include <string_view>

namespace nutmeg::ai {

// The AI parameters a difficulty and a mentality resolve to.
struct Style {
    DecisionStyle decision;
    float press = 1.0f;
};

Style styleFor(Difficulty difficulty, Mentality mentality);

std::optional<Difficulty> parseDifficulty(std::string_view name);

// A team's tactical setup as the lineup input carries it.
struct Tactics {
    Formation formation = Formation::F442;
    Mentality mentality = Mentality::Balanced;
    Difficulty difficulty = Difficulty::Normal;
};

// Maps the runner's `formation` and `mentality` strings (and an optional
// difficulty name) onto Tactics. Nullopt when any of them is unknown.
std::optional<Tactics> parseTactics(std::string_view formation, std::string_view mentality, std::string_view difficulty = "normal");

// A Decider already set up for `tactics`. `seed` feeds its decision noise.
Decider makeDecider(const Tactics& tactics, std::uint64_t seed);

// As the full drive in bot.h, with pressing scaled by `tactics`.
TeamIntent drive(const gameplay::World& world, physics::Side side, const Tactics& tactics, BallControl control,
    gameplay::PlayerInputs& inputs, gameplay::PlayerActions& actions, Decider& decider);

}

#include "nutmeg/ai/tactics.h"

namespace nutmeg::ai {

Style styleFor(Difficulty difficulty, Mentality mentality)
{
    namespace t = tactics_tuning;
    Style style;
    style.decision.riskAversion = t::riskAversion(mentality) * t::difficultyRisk(difficulty);
    style.decision.noise = t::decisionNoise(difficulty);
    style.decision.reactionTicks = t::reactionTicks(difficulty);
    style.press = t::mentalityPress(mentality) * t::difficultyPress(difficulty);
    return style;
}

std::optional<Difficulty> parseDifficulty(std::string_view name)
{
    if (name == "easy") return Difficulty::Easy;
    if (name == "normal") return Difficulty::Normal;
    if (name == "hard") return Difficulty::Hard;
    return std::nullopt;
}

std::optional<Tactics> parseTactics(std::string_view formation, std::string_view mentality, std::string_view difficulty)
{
    const auto f = parseFormation(formation);
    const auto m = parseMentality(mentality);
    const auto d = parseDifficulty(difficulty);
    if (!f || !m || !d) {
        return std::nullopt;
    }
    return Tactics{*f, *m, *d};
}

Decider makeDecider(const Tactics& tactics, std::uint64_t seed)
{
    return Decider(styleFor(tactics.difficulty, tactics.mentality).decision, seed);
}

TeamIntent drive(const gameplay::World& world, physics::Side side, const Tactics& tactics, BallControl control,
    gameplay::PlayerInputs& inputs, gameplay::PlayerActions& actions, Decider& decider)
{
    TeamInput in = inputFor(world, side, tactics.formation, tactics.mentality, control);
    in.pressScale = styleFor(tactics.difficulty, tactics.mentality).press;
    const TeamIntent intent = planTeam(in);
    const std::size_t own = side == physics::Side::Home ? 0 : gameplay::kPlayersPerSide;
    for (std::size_t i = 0; i < kOutfielders; ++i) {
        inputs[own + 1 + i] = moveToward(world.players()[own + 1 + i], intent.targets[i]);
    }
    const std::uint32_t chaser = looseBallChaser(world, side);
    if (chaser != gameplay::kNoPlayer) {
        inputs[chaser] = moveToward(world.players()[chaser], {physics::xy(world.ball().position), tuning::kPressUrgency});
    }
    const CarrierPlan plan = decider.plan(world, side);
    if (plan.player == gameplay::kNoPlayer) {
        return intent;
    }
    actions[plan.player] = plan.request;
    if (plan.steer) {
        inputs[plan.player] = moveToward(world.players()[plan.player], {plan.destination, tuning::kSprintUrgency});
    }
    return intent;
}

}

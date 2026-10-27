#pragma once

#include "nutmeg/ai/tuning.h"

namespace nutmeg::ai {

enum class Difficulty { Easy, Normal, Hard };

// Every number that turns a difficulty and a mentality into AI parameters.
// Risk aversion multiplies the utility model's risk terms; reaction is extra
// ticks before a carrier decides; noise is the half-width of the uniform jitter
// added to each candidate's utility; press scales how far from the ball a
// player will go to close it down.
namespace tactics_tuning {

constexpr float riskAversion(Mentality m)
{
    switch (m) {
    case Mentality::Attacking: return 0.5f;
    case Mentality::Defensive: return 4.0f;
    case Mentality::Balanced: break;
    }
    return 1.8f;
}

constexpr float mentalityPress(Mentality m)
{
    switch (m) {
    case Mentality::Attacking: return 1.25f;
    case Mentality::Defensive: return 0.7f;
    case Mentality::Balanced: break;
    }
    return 1.0f;
}

constexpr int reactionTicks(Difficulty d)
{
    switch (d) {
    case Difficulty::Easy: return 14;
    case Difficulty::Hard: return 0;
    case Difficulty::Normal: break;
    }
    return 5;
}

constexpr float decisionNoise(Difficulty d)
{
    switch (d) {
    case Difficulty::Easy: return 0.12f;
    case Difficulty::Hard: return 0.0f;
    case Difficulty::Normal: break;
    }
    return 0.04f;
}

constexpr float difficultyPress(Difficulty d)
{
    switch (d) {
    case Difficulty::Easy: return 0.7f;
    case Difficulty::Hard: return 1.2f;
    case Difficulty::Normal: break;
    }
    return 1.0f;
}

// Multiplies the mentality's risk aversion; an easy side is a little more reckless.
constexpr float difficultyRisk(Difficulty d)
{
    switch (d) {
    case Difficulty::Easy: return 0.85f;
    case Difficulty::Hard: return 1.0f;
    case Difficulty::Normal: break;
    }
    return 0.95f;
}

}

}

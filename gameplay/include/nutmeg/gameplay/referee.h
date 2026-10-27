#pragma once

#include "nutmeg/core/rng.h"
#include "nutmeg/gameplay/events.h"
#include "nutmeg/gameplay/player.h"
#include "nutmeg/physics/vec.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace nutmeg::gameplay {

enum class RestartKind : std::uint8_t { DirectFreeKick, IndirectFreeKick, Penalty, Kickoff, ThrowIn, GoalKick, Corner };

// Where and by whom play restarts after a stoppage. `side` is the team that
// takes it; `taker` is the player stood at the ball, set when the restart is applied.
struct Restart {
    RestartKind kind = RestartKind::DirectFreeKick;
    physics::Side side = physics::Side::Home;
    physics::Vec2 spot;
    std::uint32_t taker = kNoPlayer;

    bool operator==(const Restart&) const = default;
};

// Tuning for the referee. Times are ticks, speeds m/s.
namespace referee {

constexpr float kYellowBase = 0.12f;
constexpr float kYellowAggression = 0.5f;
constexpr float kRedBase = 0.01f;
constexpr float kRedAggression = 0.05f;

constexpr int kAdvantageTicks = 120;
constexpr float kAdvantageMinForwardSpeed = 1.5f;

}

// True if `attacker` is in an offside position when the ball is played from
// `ballX`: in the opponents' half, nearer their goal line than the ball and
// than the second-last opponent. Sent-off players do not count as defenders.
bool inOffsidePosition(const Player* players, std::size_t count, const Player& attacker, float ballX);

// The state of the match the referee reads each tick.
struct Situation {
    const Player* players = nullptr;
    std::size_t count = 0;
    std::uint32_t possessor = kNoPlayer;
    std::uint64_t tick = 0;
    // The ball was kicked from a restart that cannot be offside.
    bool offsideExempt = false;
};

class Referee {
public:
    Referee(Rng rng, std::size_t playerCount);

    // Reads the events of one tick and appends its rulings to them: fouls,
    // cards, offside and advantage. Returns the restart when play must stop.
    std::optional<Restart> review(const Situation& situation, std::vector<MatchEvent>& events);

    int yellowCards(std::uint32_t player) const { return yellows_[player]; }
    bool sentOff(std::uint32_t player) const { return sentOff_[player] != 0; }
    // Swaps the disciplinary record of two player slots, as when the teams change ends.
    void exchange(std::uint32_t a, std::uint32_t b);
    bool advantageActive() const { return advantage_.active; }
    std::uint64_t hash() const;

private:
    enum class Card : std::uint8_t { None, Yellow, Red };

    struct Advantage {
        bool active = false;
        std::uint32_t fouler = kNoPlayer;
        std::uint32_t victim = kNoPlayer;
        Card card = Card::None;
        Restart restart;
        std::uint64_t expires = 0;
    };

    std::optional<Restart> onFoul(const MatchEvent& event, const Situation& situation, std::vector<MatchEvent>& rulings);
    void markPlayed(const MatchEvent& event, const Situation& situation);
    std::optional<Restart> onTouch(const MatchEvent& event, const Situation& situation, std::vector<MatchEvent>& rulings);
    std::optional<Restart> watchAdvantage(const Situation& situation, std::vector<MatchEvent>& rulings);
    std::optional<Restart> endAdvantage(bool kept, std::uint64_t tick, const Situation& situation, std::vector<MatchEvent>& rulings);
    void showCard(std::uint32_t player, Card card, const Situation& situation, std::vector<MatchEvent>& rulings);
    void clearPlayed();

    Rng rng_;
    std::vector<std::uint8_t> yellows_;
    std::vector<std::uint8_t> sentOff_;
    std::vector<std::uint8_t> offsideAtPlay_;
    bool played_ = false;
    std::uint32_t playedBy_ = kNoPlayer;
    Advantage advantage_;
};

}

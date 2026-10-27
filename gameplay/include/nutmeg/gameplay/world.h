#pragma once

#include "nutmeg/controls/control_state.h"
#include "nutmeg/core/input.h"
#include "nutmeg/core/rng.h"
#include "nutmeg/gameplay/actions.h"
#include "nutmeg/gameplay/events.h"
#include "nutmeg/gameplay/player.h"
#include "nutmeg/gameplay/referee.h"
#include "nutmeg/physics/ball.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace nutmeg::gameplay {

inline constexpr std::size_t kPlayersPerSide = 11;
inline constexpr std::size_t kPlayerCount = 2 * kPlayersPerSide;

using PlayerInputs = std::array<InputFrame, kPlayerCount>;
using PlayerActions = std::array<ActionRequest, kPlayerCount>;

// Players 0..10 are Home, 11..21 Away. Within a side index 0 is the keeper.
class World {
public:
    // Default-rated players in a 4-4-2 on their own halves.
    explicit World(std::uint64_t seed = 0);
    World(const std::array<schema::PlayerProfile, kPlayerCount>& profiles, std::uint64_t seed = 0);

    // Input is relative to the attacking direction: moveY is towards the
    // opponent's goal, moveX towards the right-hand touchline as the player
    // faces it. Sprint is a button.
    // The buttons of each frame become actions for that player. A request in
    // `actions` replaces the one the buttons would make, which is how the AI acts.
    void step(const PlayerInputs& inputs);
    void step(const PlayerInputs& inputs, const PlayerActions& actions);

    const std::array<Player, kPlayerCount>& players() const { return players_; }
    std::array<Player, kPlayerCount>& players() { return players_; }
    const physics::Ball& ball() const { return ball_; }
    physics::Ball& ball() { return ball_; }
    std::uint64_t tick() const { return tick_; }
    // Index of the player who has the ball under control, or kNoPlayer.
    std::uint32_t possessor() const { return possessor_; }
    std::uint32_t lastToucher() const { return lastToucher_; }
    // True from the tick the ball goes out or into the goal until it is back in play.
    bool ballDead() const { return ballDead_; }
    const Referee& referee() const { return referee_; }
    // The free kick or penalty awaiting its taker. The ball is held on the
    // spot, and only the taking side may kick it, until it is kicked.
    const std::optional<Restart>& restart() const { return restart_; }
    // What happened in the most recent step.
    const std::vector<MatchEvent>& events() const { return events_; }
    std::uint64_t hash() const;

private:
    void collectRequests(const PlayerInputs& inputs, const PlayerActions& actions, PlayerActions& requests);
    void resolveActions(const PlayerActions& requests);
    void kick(std::size_t index, const ActionRequest& request);
    void tackle(std::size_t index);
    void resolveLooseBall();
    void attachBall(const Player& carrier);
    void deflect(Player& player);
    void checkBallStatus();
    void emit(MatchEvent event);
    void review();
    void applyRestart(const Restart& restart);
    void sendOff(Player& player);
    bool barredByRestart(const Player& player) const;

    std::array<Player, kPlayerCount> players_;
    physics::Ball ball_;
    std::uint64_t tick_ = 0;
    Rng rng_;
    Referee referee_;
    std::optional<Restart> restart_;
    std::array<controls::ControlState, kPlayerCount> controls_;
    std::uint32_t possessor_ = kNoPlayer;
    std::uint32_t lastToucher_ = kNoPlayer;
    bool ballDead_ = false;
    std::vector<MatchEvent> events_;
};

}

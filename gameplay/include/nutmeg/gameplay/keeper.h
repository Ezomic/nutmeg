#pragma once

#include "nutmeg/core/input.h"
#include "nutmeg/core/rng.h"
#include "nutmeg/gameplay/actions.h"
#include "nutmeg/gameplay/events.h"
#include "nutmeg/gameplay/player.h"
#include "nutmeg/physics/ball.h"
#include "nutmeg/physics/pitch.h"
#include "nutmeg/physics/vec.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace nutmeg::gameplay {

inline constexpr std::size_t kKeeperSlots = 2;
inline constexpr std::size_t kKeeperSideSize = 11;
inline constexpr std::size_t kKeeperPlayerCount = kKeeperSlots * kKeeperSideSize;

// Tuning for the goalkeeper. Distances are metres, speeds m/s, times ticks.
// Skill terms map a goalkeeper attribute 1..100 to 0..1.
namespace keeper {

// Where the keeper stands: on the line from the goal centre to the ball, at a
// depth that grows with the distance of the ball.
constexpr float kDepthMin = 0.8f;
constexpr float kDepthMax = 6.5f;
constexpr float kDepthBase = 0.8f;
constexpr float kDepthPerMetre = 0.12f;
constexpr float kDepthSkillLow = 0.6f;
constexpr float kLeadSeconds = 0.4f;
constexpr float kArriveRadius = 1.5f;
constexpr float kSprintDistance = 4.0f;

// A shot is a ball heading for the goal frame faster than this.
constexpr float kThreatSpeed = 9.0f;
constexpr float kThreatHold = 3.0f;
constexpr float kThreatMargin = 1.0f;
constexpr int kPredictTicks = 180;
constexpr float kRedirectCosine = 0.97f;

constexpr int kReactionSlow = 22;
constexpr int kReactionFast = 8;
constexpr float kReadError = 1.2f;
constexpr float kDiveSpeedSlow = 4.5f;
constexpr float kDiveSpeedFast = 9.5f;

constexpr float kReachBase = 0.8f;
constexpr float kReachDive = 1.1f;
constexpr float kReachHeightBase = 1.7f;
constexpr float kReachHeightDive = 0.9f;

constexpr float kSaveBase = 0.86f;
constexpr float kSaveReflexes = 0.30f;
constexpr float kSavePositioning = 0.10f;
constexpr float kSaveSpeedEasy = 12.0f;
constexpr float kSaveSpeedRange = 22.0f;
constexpr float kSaveSpeed = 0.30f;
constexpr float kSaveStretch = 0.25f;
constexpr float kSaveHeight = 0.25f;
constexpr float kSaveMin = 0.02f;
constexpr float kSaveMax = 0.97f;
constexpr float kSaveWindow = 1.2f;
constexpr int kSaveCooldown = 30;

constexpr float kCatchBase = 0.40f;
constexpr float kCatchHandling = 0.50f;
constexpr float kCatchSpeed = 0.35f;
constexpr float kCatchStretch = 0.15f;
constexpr float kParryKeepLoose = 0.55f;
constexpr float kParryKeepSkilled = 0.25f;
constexpr float kParryCos = 0.64f;
constexpr float kParrySin = 0.77f;
constexpr float kParryLift = 2.0f;
constexpr float kParryLiftMax = 3.0f;

// Claims: crosses and loose balls inside the penalty area.
constexpr int kClaimPeriod = 6;
constexpr float kClaimMaxDistance = 40.0f;
constexpr float kClaimHeight = 2.6f;
constexpr float kLooseHeight = 0.5f;
constexpr float kLooseSpeed = 6.0f;
constexpr int kClaimBallSlack = 20;
constexpr int kClaimOpponentSlack = 10;
constexpr float kClaimReachBase = 0.7f;
constexpr float kClaimReachDive = 0.5f;
constexpr float kClaimBase = 0.75f;
constexpr float kClaimHandling = 0.40f;
constexpr float kClaimSpeed = 0.30f;
constexpr float kClaimPressureRadius = 2.5f;
constexpr float kClaimPressure = 0.15f;
constexpr int kClaimCooldown = 20;

// Distribution.
constexpr int kHoldTicks = 75;
constexpr float kHoldBallDistance = 0.45f;
constexpr float kRangeShort = 25.0f;
constexpr float kRangeLong = 55.0f;
constexpr float kMinTarget = 8.0f;
constexpr float kGroundMax = 22.0f;
constexpr float kForwardWeight = 1.0f;
constexpr float kOpenWeight = 0.5f;
constexpr float kOpenRadius = 10.0f;
constexpr float kScatterBest = 0.02f;
constexpr float kScatterWorst = 0.15f;
constexpr float kClearanceY = 20.0f;

}

// What the keeper wants this tick in place of the stick and the sprint button.
struct KeeperDrive {
    bool active = false;
    physics::Vec2 intent;
    bool sprint = false;
};

// The parts of the world the keepers read and write.
struct KeeperSituation {
    std::array<Player, kKeeperPlayerCount>& players;
    physics::Ball& ball;
    std::uint32_t& possessor;
    std::uint32_t& lastToucher;
    bool ballDead;
    std::uint64_t tick;
    std::vector<MatchEvent>& events;
};

// Where a keeper of `defending` stands to cover a ball at `ball` moving at `ballVelocity`.
physics::Vec2 keeperGuardPoint(physics::Side defending, physics::Vec2 ball, physics::Vec2 ballVelocity, int positioning);

// True when the frame carries input, in which case a person is playing the keeper.
bool keeperIsManual(const InputFrame& input);

// Both goalkeepers. Index 0 of each side is the keeper, as in World.
class Keepers {
public:
    explicit Keepers(Rng rng);

    // Before movement: sees shots and crosses coming, decides where each keeper
    // goes, and asks for a kick when one holds the ball.
    void plan(const KeeperSituation& situation, const std::array<InputFrame, kKeeperPlayerCount>& inputs, std::array<ActionRequest, kKeeperPlayerCount>& requests);
    // The movement plan() made for the player, or nullptr when it is not a keeper or is played by a person.
    const KeeperDrive* drive(std::size_t player) const;
    // After movement and before the ball moves: dives, saves and claims.
    void resolve(const KeeperSituation& situation);
    // A keeper in his own penalty area cannot be tackled.
    bool protects(const Player& player) const;

    std::uint64_t hash() const;

private:
    struct State {
        bool evaluated = false;
        bool engaged = false;
        int elapsed = 0;
        int reaction = 0;
        float targetY = 0.0f;
        physics::Vec2 shotDirection;
        bool claiming = false;
        physics::Vec2 claimPoint;
        int hold = 0;
        bool manual = false;
    };

    void detectShot(const KeeperSituation& situation, std::size_t slot);
    void detectClaim(const KeeperSituation& situation, std::size_t slot);
    KeeperDrive positioningDrive(const KeeperSituation& situation, std::size_t slot, bool claimed) const;
    ActionRequest distribution(const KeeperSituation& situation, std::size_t slot);
    void dive(Player& keeper, State& state) const;
    void attemptSave(const KeeperSituation& situation, std::size_t slot);
    void attemptClaim(const KeeperSituation& situation, std::size_t slot);
    void hold(const KeeperSituation& situation, Player& keeper);
    void parry(const KeeperSituation& situation, Player& keeper, float keep);

    Rng rng_;
    std::array<State, kKeeperSlots> states_;
    std::array<KeeperDrive, kKeeperSlots> drives_;
};

}

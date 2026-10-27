#include "nutmeg/gameplay/keeper.h"

#include "nutmeg/core/hash.h"
#include "nutmeg/gameplay/locomotion.h"

#include <algorithm>
#include <bit>

namespace nutmeg::gameplay {

namespace {

using physics::Side;
using physics::Vec2;
using physics::Vec3;
namespace tune = keeper;

float lerp(float a, float b, float t) { return a + (b - a) * t; }

float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

float clampTo(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

std::uint64_t bits(float v) { return mix64(std::bit_cast<std::uint32_t>(v)); }

std::size_t playerOf(std::size_t slot) { return slot * kKeeperSideSize; }

Side sideOf(std::size_t slot) { return slot == 0 ? Side::Home : Side::Away; }

// +1 when the goal this side defends is at -x (Home), so "out" is +x.
float outwards(Side defending) { return defending == Side::Home ? 1.0f : -1.0f; }

float speedFactor(float speed)
{
    return clamp01((speed - tune::kSaveSpeedEasy) / tune::kSaveSpeedRange);
}

float reachRadius(const schema::GoalkeeperAttributes& g)
{
    return tune::kReachBase + tune::kReachDive * skill(g.diving);
}

}

Vec2 keeperGuardPoint(Side defending, Vec2 ball, Vec2 ballVelocity, int positioning)
{
    const float skillPos = skill(positioning);
    const float out = outwards(defending);
    const Vec2 goal = physics::pitch::goalCentre(defending);

    const Vec2 lead = ballVelocity * (tune::kLeadSeconds * skillPos);
    const Vec2 seen = ball + lead;
    const Vec2 toBall{std::max((seen.x - goal.x) * out, 0.0f) * out, seen.y - goal.y};
    const float dist = physics::length(toBall);
    const Vec2 unit = dist > 1e-3f ? toBall * (1.0f / dist) : Vec2{out, 0.0f};

    const float depth = clampTo((tune::kDepthBase + tune::kDepthPerMetre * dist) * lerp(tune::kDepthSkillLow, 1.0f, skillPos), tune::kDepthMin, tune::kDepthMax);
    return goal + unit * depth;
}

bool keeperIsManual(const InputFrame& input)
{
    return input.buttons != 0 || input.moveX != 0.0f || input.moveY != 0.0f;
}

Keepers::Keepers(Rng rng)
    : rng_(rng)
{
}

const KeeperDrive* Keepers::drive(std::size_t player) const
{
    if (player % kKeeperSideSize != 0) {
        return nullptr;
    }
    const KeeperDrive& d = drives_[player / kKeeperSideSize];
    return d.active ? &d : nullptr;
}

bool Keepers::protects(const Player& player) const
{
    return player.id % kKeeperSideSize == 0 && physics::pitch::inPenaltyArea(player.position, player.side);
}

void Keepers::plan(const KeeperSituation& situation, const std::array<InputFrame, kKeeperPlayerCount>& inputs, std::array<ActionRequest, kKeeperPlayerCount>& requests)
{
    for (std::size_t slot = 0; slot < kKeeperSlots; ++slot) {
        const std::size_t index = playerOf(slot);
        State& state = states_[slot];
        state.manual = keeperIsManual(inputs[index]);
        drives_[slot] = {};

        if (situation.ballDead) {
            state.evaluated = false;
            state.engaged = false;
            state.claiming = false;
            continue;
        }

        if (situation.possessor == index) {
            state.engaged = false;
            state.claiming = false;
            ++state.hold;
            if (!state.manual) {
                drives_[slot].active = true;
                if (state.hold >= tune::kHoldTicks && requests[index].kind == ActionKind::None) {
                    requests[index] = distribution(situation, slot);
                }
            }
            continue;
        }
        state.hold = 0;

        detectShot(situation, slot);
        if (state.engaged) {
            ++state.elapsed;
        } else {
            detectClaim(situation, slot);
        }
        if (!state.manual) {
            drives_[slot] = positioningDrive(situation, slot, state.claiming);
        }
    }
}

void Keepers::detectShot(const KeeperSituation& situation, std::size_t slot)
{
    State& state = states_[slot];
    const Player& keeper = situation.players[playerOf(slot)];
    const physics::Ball& ball = situation.ball;
    const Side defending = keeper.side;
    const float out = outwards(defending);

    const Vec2 planar = physics::xy(ball.velocity);
    const float speed = physics::length(planar);
    const float towardsGoal = -planar.x * out;
    const bool opponentBall = situation.lastToucher < kKeeperPlayerCount && situation.players[situation.lastToucher].side != defending;
    const bool heading = situation.possessor == kNoPlayer && opponentBall && towardsGoal >= (state.engaged ? tune::kThreatHold : tune::kThreatSpeed);

    if (!heading) {
        state.evaluated = false;
        state.engaged = false;
        return;
    }
    if (state.engaged) {
        const float cosine = physics::dot(planar, state.shotDirection) / speed;
        if (cosine < tune::kRedirectCosine) {
            state.evaluated = false;
            state.engaged = false;
        }
    }
    if (state.engaged || state.evaluated) {
        return;
    }
    state.evaluated = true;

    physics::Ball flight = ball;
    const float line = physics::pitch::goalLineX(defending);
    float planeY = 0.0f;
    bool atPlane = false;
    for (int i = 0; i < tune::kPredictTicks; ++i) {
        flight.step();
        if (!atPlane && (flight.position.x - keeper.position.x) * out <= 0.0f) {
            atPlane = true;
            planeY = flight.position.y;
        }
        if ((flight.position.x - line) * out <= 0.0f) {
            const bool onFrame = std::abs(flight.position.y) <= physics::pitch::kHalfGoalWidth + tune::kThreatMargin && flight.position.z <= physics::pitch::kGoalHeight + tune::kThreatMargin;
            if (onFrame) {
                const auto& g = keeper.profile.goalkeeper;
                const float error = (rng_.nextFloat() * 2.0f - 1.0f) * tune::kReadError * (1.0f - skill(g.positioning));
                state.engaged = true;
                state.elapsed = 0;
                state.claiming = false;
                state.reaction = static_cast<int>(lerp(static_cast<float>(tune::kReactionSlow), static_cast<float>(tune::kReactionFast), skill(g.reflexes)) + 0.5f);
                state.targetY = (atPlane ? planeY : flight.position.y) + error;
                state.shotDirection = planar;
            }
            return;
        }
    }
}

void Keepers::detectClaim(const KeeperSituation& situation, std::size_t slot)
{
    State& state = states_[slot];
    if (situation.tick % tune::kClaimPeriod != 0) {
        return;
    }
    state.claiming = false;

    const Player& keeper = situation.players[playerOf(slot)];
    const physics::Ball& ball = situation.ball;
    const Side defending = keeper.side;
    if (situation.possessor != kNoPlayer || situation.lastToucher == keeper.id || physics::distance(physics::xy(ball.position), physics::pitch::goalCentre(defending)) > tune::kClaimMaxDistance) {
        return;
    }

    Vec2 point = physics::xy(ball.position);
    int ballTicks = 0;
    const bool airborne = ball.position.z > tune::kLooseHeight;
    if (airborne) {
        physics::Ball flight = ball;
        bool found = false;
        for (int i = 1; i <= tune::kPredictTicks; ++i) {
            flight.step();
            if (flight.velocity.z < 0.0f && flight.position.z <= tune::kClaimHeight) {
                point = physics::xy(flight.position);
                ballTicks = i;
                found = true;
                break;
            }
        }
        if (!found) {
            return;
        }
    } else if (physics::length(physics::xy(ball.velocity)) > tune::kLooseSpeed) {
        return;
    }
    if (!physics::pitch::inPenaltyArea(point, defending)) {
        return;
    }

    constexpr float step = locomotion::kStepSeconds;
    const float keeperTicks = physics::distance(keeper.position, point) / (locomotion::sprintSpeed(keeper.profile.attributes.pace) * step);
    float opponentTicks = 1e9f;
    for (const Player& p : situation.players) {
        if (p.side != defending) {
            opponentTicks = std::min(opponentTicks, physics::distance(p.position, point) / (locomotion::sprintSpeed(p.profile.attributes.pace) * step));
        }
    }
    const bool beatsBall = !airborne || keeperTicks <= static_cast<float>(ballTicks + tune::kClaimBallSlack);
    if (beatsBall && keeperTicks <= opponentTicks + static_cast<float>(tune::kClaimOpponentSlack)) {
        state.claiming = true;
        state.claimPoint = point;
    }
}

KeeperDrive Keepers::positioningDrive(const KeeperSituation& situation, std::size_t slot, bool claimed) const
{
    const Player& keeper = situation.players[playerOf(slot)];
    const State& state = states_[slot];
    Vec2 target;
    if (claimed) {
        target = state.claimPoint;
    } else {
        target = keeperGuardPoint(keeper.side, physics::xy(situation.ball.position), physics::xy(situation.ball.velocity), keeper.profile.goalkeeper.positioning);
    }

    const Vec2 delta = target - keeper.position;
    const float dist = physics::length(delta);
    KeeperDrive drive;
    drive.active = true;
    if (dist > 1e-3f) {
        drive.intent = delta * (1.0f / dist) * std::min(1.0f, dist / tune::kArriveRadius);
    }
    drive.sprint = claimed || dist > tune::kSprintDistance;
    return drive;
}

ActionRequest Keepers::distribution(const KeeperSituation& situation, std::size_t slot)
{
    const Player& keeper = situation.players[playerOf(slot)];
    const auto& g = keeper.profile.goalkeeper;
    const float kicking = skill(g.kicking);
    const float range = lerp(tune::kRangeShort, tune::kRangeLong, kicking);
    const float forward = outwards(keeper.side);

    Vec2 aim{0.0f, keeper.position.y < 0.0f ? tune::kClearanceY : -tune::kClearanceY};
    float best = -1e9f;
    for (const Player& mate : situation.players) {
        if (mate.side != keeper.side || mate.id == keeper.id) {
            continue;
        }
        const float dist = physics::distance(mate.position, keeper.position);
        if (dist < tune::kMinTarget || dist > range) {
            continue;
        }
        float nearest = tune::kOpenRadius;
        for (const Player& other : situation.players) {
            if (other.side != keeper.side) {
                nearest = std::min(nearest, physics::distance(other.position, mate.position));
            }
        }
        const float progress = (mate.position.x - keeper.position.x) * forward / physics::pitch::kLength;
        const float score = tune::kForwardWeight * progress + tune::kOpenWeight * nearest / tune::kOpenRadius;
        if (score > best) {
            best = score;
            aim = mate.position;
        }
    }

    const float distance = physics::distance(aim, keeper.position);
    const float scatter = lerp(tune::kScatterWorst, tune::kScatterBest, kicking) * distance;
    aim.x += (rng_.nextFloat() * 2.0f - 1.0f) * scatter;
    aim.y += (rng_.nextFloat() * 2.0f - 1.0f) * scatter;

    ActionRequest request;
    request.aim = aim;
    if (distance <= tune::kGroundMax) {
        request.kind = ActionKind::Pass;
        request.power = passPowerForDistance(distance);
    } else {
        request.kind = ActionKind::Cross;
    }
    return request;
}

void Keepers::resolve(const KeeperSituation& situation)
{
    if (situation.ballDead) {
        return;
    }
    for (std::size_t slot = 0; slot < kKeeperSlots; ++slot) {
        Player& keeper = situation.players[playerOf(slot)];
        State& state = states_[slot];
        if (situation.possessor == keeper.id) {
            continue;
        }
        if (state.engaged) {
            if (!state.manual && state.elapsed >= state.reaction) {
                dive(keeper, state);
            }
            attemptSave(situation, slot);
        } else if (state.claiming) {
            attemptClaim(situation, slot);
        }
    }
}

void Keepers::dive(Player& keeper, State& state) const
{
    const auto& g = keeper.profile.goalkeeper;
    const float speed = lerp(tune::kDiveSpeedSlow, tune::kDiveSpeedFast, skill(g.diving));
    const float maxStep = speed * locomotion::kStepSeconds;
    const float delta = state.targetY - keeper.position.y;
    const float moved = clampTo(delta, -maxStep, maxStep);
    keeper.position.y += moved;
    keeper.velocity = {0.0f, moved / locomotion::kStepSeconds};
}

void Keepers::hold(const KeeperSituation& situation, Player& keeper)
{
    const float out = outwards(keeper.side);
    keeper.facing = keeper.side == Side::Home ? 0.0f : 3.14159265f;
    keeper.velocity = {};
    situation.possessor = keeper.id;
    situation.lastToucher = keeper.id;
    situation.ball.position = {keeper.position.x + out * tune::kHoldBallDistance, keeper.position.y, physics::ball::kRadius};
    situation.ball.velocity = {};
    situation.ball.spin = {};
}

void Keepers::parry(const KeeperSituation& situation, Player& keeper, float keep)
{
    physics::Ball& ball = situation.ball;
    const float out = outwards(keeper.side);
    const float speed = physics::length(ball.velocity) * keep;
    float lateral = ball.position.y - keeper.position.y;
    if (std::abs(lateral) < 0.05f) {
        lateral = rng_.nextFloat() < 0.5f ? -1.0f : 1.0f;
    }
    const float side = lateral < 0.0f ? -1.0f : 1.0f;
    const float lift = std::min(std::max(ball.velocity.z, 0.0f) + tune::kParryLift, tune::kParryLiftMax);
    ball.velocity = {out * tune::kParryCos * speed, side * tune::kParrySin * speed, lift};
    situation.lastToucher = keeper.id;
}

void Keepers::attemptSave(const KeeperSituation& situation, std::size_t slot)
{
    Player& keeper = situation.players[playerOf(slot)];
    if (keeper.cooldown > 0) {
        return;
    }
    const auto& g = keeper.profile.goalkeeper;
    const physics::Ball& ball = situation.ball;
    const float reach = reachRadius(g);
    const float height = tune::kReachHeightBase + tune::kReachHeightDive * skill(g.diving);
    const float speed = physics::length(ball.velocity);
    const float perTick = physics::length(physics::xy(ball.velocity)) * locomotion::kStepSeconds;
    const float ahead = (ball.position.x - keeper.position.x) * outwards(keeper.side);
    const float away = std::abs(ball.position.y - keeper.position.y);
    if (ahead > tune::kSaveWindow || ahead < -perTick || away > reach || ball.position.z > height) {
        return;
    }

    const float hard = speedFactor(speed);
    const float stretch = away / reach;
    const float p = clampTo(tune::kSaveBase + tune::kSaveReflexes * (skill(g.reflexes) - 0.5f) + tune::kSavePositioning * (skill(g.positioning) - 0.5f) - tune::kSaveSpeed * hard - tune::kSaveStretch * stretch - tune::kSaveHeight * (ball.position.z / height), tune::kSaveMin, tune::kSaveMax);

    keeper.cooldown = tune::kSaveCooldown;
    if (rng_.nextFloat() >= p) {
        return;
    }

    MatchEvent event;
    event.kind = EventKind::Save;
    event.player = keeper.id;
    event.other = situation.lastToucher;
    event.side = keeper.side;

    const float catchP = clampTo(tune::kCatchBase + tune::kCatchHandling * skill(g.handling) - tune::kCatchSpeed * hard - tune::kCatchStretch * stretch, 0.0f, 1.0f);
    if (rng_.nextFloat() < catchP) {
        event.value = 1.0f;
        hold(situation, keeper);
    } else {
        event.value = 0.0f;
        parry(situation, keeper, lerp(tune::kParryKeepLoose, tune::kParryKeepSkilled, skill(g.handling)));
    }
    event.tick = situation.tick;
    situation.events.push_back(event);
    states_[slot].engaged = false;
    states_[slot].evaluated = false;
}

void Keepers::attemptClaim(const KeeperSituation& situation, std::size_t slot)
{
    Player& keeper = situation.players[playerOf(slot)];
    if (keeper.cooldown > 0 || situation.lastToucher == keeper.id) {
        return;
    }
    const auto& g = keeper.profile.goalkeeper;
    const physics::Ball& ball = situation.ball;
    const float reach = tune::kClaimReachBase + tune::kClaimReachDive * skill(g.diving);
    if (physics::distance(keeper.position, physics::xy(ball.position)) > reach || ball.position.z > tune::kClaimHeight) {
        return;
    }

    float pressure = 0.0f;
    for (const Player& p : situation.players) {
        if (p.side != keeper.side && physics::distance(p.position, keeper.position) <= tune::kClaimPressureRadius) {
            pressure += tune::kClaimPressure;
        }
    }
    const float speed = physics::length(ball.velocity);
    const float p = clampTo(tune::kClaimBase + tune::kClaimHandling * (skill(g.handling) - 0.5f) - tune::kClaimSpeed * speedFactor(speed) - pressure, tune::kSaveMin, tune::kSaveMax);

    MatchEvent event;
    event.kind = EventKind::Claim;
    event.tick = situation.tick;
    event.player = keeper.id;
    event.side = keeper.side;

    if (rng_.nextFloat() < p) {
        event.value = 1.0f;
        hold(situation, keeper);
    } else {
        event.value = 0.0f;
        keeper.cooldown = tune::kClaimCooldown;
        parry(situation, keeper, lerp(tune::kParryKeepLoose, tune::kParryKeepSkilled, skill(g.handling)));
    }
    situation.events.push_back(event);
    states_[slot].claiming = false;
}

std::uint64_t Keepers::hash() const
{
    std::uint64_t h = mix64(rng_.state());
    for (const State& s : states_) {
        h = hashCombine(h, mix64((s.evaluated ? 1u : 0u) | (s.engaged ? 2u : 0u) | (s.claiming ? 4u : 0u)));
        h = hashCombine(h, mix64(static_cast<std::uint64_t>(s.elapsed)));
        h = hashCombine(h, mix64(static_cast<std::uint64_t>(s.hold)));
        h = hashCombine(h, bits(s.targetY));
        h = hashCombine(h, bits(s.claimPoint.x));
        h = hashCombine(h, bits(s.claimPoint.y));
    }
    return h;
}

}

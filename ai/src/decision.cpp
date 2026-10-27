#include "nutmeg/ai/decision.h"

#include "nutmeg/ai/bot.h"
#include "nutmeg/ai/utility_tuning.h"
#include "nutmeg/core/dmath.h"

#include <algorithm>
#include <cmath>

namespace nutmeg::ai {

namespace {

namespace tune = utility::tuning;
using gameplay::ActionKind;
using gameplay::ActionRequest;
using gameplay::kNoPlayer;
using gameplay::kPlayersPerSide;
using gameplay::Player;
using physics::Side;
using physics::Vec2;

float clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }
float lerp(float a, float b, float t) { return a + (b - a) * t; }
float sideSign(Side s) { return s == Side::Home ? 1.0f : -1.0f; }
std::size_t firstIndex(Side s) { return s == Side::Home ? 0 : kPlayersPerSide; }

struct Context {
    const gameplay::World& world;
    const Player& carrier;
    Side side;
    float sign;
    Vec2 goal;
    float pressure;
    // World x of the opponent's offside line, in the attacking direction (x * sign).
    float offside;
};

Context contextFor(const gameplay::World& world, const Player& carrier)
{
    const Side side = carrier.side;
    return {world, carrier, side, sideSign(side), physics::pitch::goalCentre(physics::opposite(side)),
        gameplay::pressureOn(carrier, world.players().data(), world.players().size()), offsideLine(world, side) * sideSign(side)};
}

float segmentDistance(Vec2 a, Vec2 b, Vec2 p)
{
    const Vec2 ab = b - a;
    const float len2 = physics::dot(ab, ab);
    const float t = len2 > 1e-6f ? clamp01(physics::dot(p - a, ab) / len2) : 0.0f;
    return physics::distance(a + ab * t, p);
}

float nearestOpponent(const Context& c, Vec2 p)
{
    const std::size_t base = firstIndex(physics::opposite(c.side));
    float best = 1e9f;
    for (std::size_t i = 0; i < kPlayersPerSide; ++i) {
        best = std::min(best, physics::distance(c.world.players()[base + i].position, p));
    }
    return best;
}

float laneClearance(const Context& c, Vec2 to)
{
    const std::size_t base = firstIndex(physics::opposite(c.side));
    float best = 1e9f;
    for (std::size_t i = 0; i < kPlayersPerSide; ++i) {
        best = std::min(best, segmentDistance(c.carrier.position, to, c.world.players()[base + i].position));
    }
    return best;
}

float markedFactor(const Context& c, Vec2 receiver)
{
    const float marker = nearestOpponent(c, receiver);
    return marker < tune::kPassMarkRadius ? 1.0f - tune::kPassReceiverMarked * (1.0f - marker / tune::kPassMarkRadius) : 1.0f;
}

float lossCost(Side side, Vec2 at)
{
    const float advance = clamp01((at.x * sideSign(side) + physics::pitch::kHalfLength) / physics::pitch::kLength);
    const float behind = 1.0f - advance;
    return tune::kLossBase + tune::kLossOwnGoal * behind * behind;
}

Candidate scored(CandidateKind kind, const ActionRequest& request, Vec2 destination, float chance, float gain, float risk)
{
    Candidate c;
    c.kind = kind;
    c.request = request;
    c.destination = destination;
    c.chance = chance;
    c.gain = gain;
    c.risk = risk * tune::kRiskAversion;
    c.utility = gain - c.risk;
    return c;
}

float goalAngle(const Context& c)
{
    const Side defending = physics::opposite(c.side);
    const Vec2 a = physics::pitch::postMinusY(defending) - c.carrier.position;
    const Vec2 b = physics::pitch::postPlusY(defending) - c.carrier.position;
    const float cross = a.x * b.y - a.y * b.x;
    return dmath::atan2(std::abs(cross), physics::dot(a, b));
}

float shotChanceFor(const Context& c)
{
    const float dist = physics::distance(c.carrier.position, c.goal);
    if (dist > tune::kXgMaxRange) {
        return 0.0f;
    }
    const float ratio = dist / tune::kXgScale;
    const float angle = std::min(goalAngle(c) / tune::kXgAngleRef, tune::kXgAngleCap);
    const float finishing = lerp(tune::kXgFinishingLow, tune::kXgFinishingHigh, gameplay::skill(c.carrier.profile.attributes.finishing));

    float blockers = 1.0f;
    const std::size_t base = firstIndex(physics::opposite(c.side));
    for (std::size_t i = 1; i < kPlayersPerSide; ++i) {
        const Vec2 p = c.world.players()[base + i].position;
        if (segmentDistance(c.carrier.position, c.goal, p) < tune::kXgBlockerWidth && physics::distance(p, c.goal) < dist) {
            blockers *= 1.0f - tune::kXgBlocker;
        }
    }
    return clamp01(tune::kXgBase / (1.0f + ratio * ratio) * angle * finishing * (1.0f - tune::kXgPressure * c.pressure) * blockers);
}

void addShot(const Context& c, std::vector<Candidate>& out)
{
    const float chance = shotChanceFor(c);
    if (chance < tune::kShotMinChance) {
        return;
    }
    ActionRequest request;
    request.kind = ActionKind::Shoot;
    request.power = tune::kShotPower;
    const float farPost = c.carrier.position.y > 0.0f ? -1.0f : 1.0f;
    request.aim = {c.goal.x, farPost * physics::pitch::kHalfGoalWidth * tune::kShotAimFraction};
    const float gain = chance * tune::kGoalValue;
    const float risk = (1.0f - chance) * tune::kShotMissLoss * lossCost(c.side, c.carrier.position);
    out.push_back(scored(CandidateKind::Shoot, request, c.carrier.position, chance, gain, risk));
}

Candidate dribbleCandidate(const Context& c)
{
    const Vec2 toGoal = c.goal - c.carrier.position;
    const float dist = physics::length(toGoal);
    const Vec2 dir = dist > 1e-3f ? toGoal * (1.0f / dist) : Vec2{c.sign, 0.0f};
    Vec2 run = c.carrier.position + dir * std::min(tune::kRunTargetDistance, dist);
    run.x = std::clamp(run.x, -physics::pitch::kHalfLength, physics::pitch::kHalfLength);
    run.y = std::clamp(run.y, -physics::pitch::kHalfWidth, physics::pitch::kHalfWidth);

    const float skillSwing = tune::kDribbleSkillSwing * (gameplay::skill(c.carrier.profile.attributes.dribbling) - 0.5f);
    const bool blocked = laneClearance(c, run) < tune::kDribbleBlockRadius;
    const float chance = (blocked ? tune::kDribbleBlockedChance : tune::kDribbleChance) + skillSwing;
    const float lift = threatAt(c.side, run) - threatAt(c.side, c.carrier.position);
    const float gain = chance * lift * (blocked ? tune::kDribbleBlockedGain : tune::kDribbleGain);
    const float risk = (1.0f - chance) * lossCost(c.side, c.carrier.position);
    return scored(CandidateKind::Dribble, ActionRequest{}, run, chance, gain, risk);
}

Candidate holdCandidate(const Context& c)
{
    return scored(CandidateKind::Hold, ActionRequest{}, c.carrier.position, 1.0f, tune::kHoldUtility, tune::kHoldPressureCost * c.pressure);
}

float passChance(const Context& c, const Player& mate, float dist, bool through)
{
    float chance = tune::kPassBaseChance - tune::kPassDistanceLoss * dist;
    chance *= lerp(tune::kPassPassingLow, tune::kPassPassingHigh, gameplay::skill(c.carrier.profile.attributes.passing));
    chance *= 1.0f - tune::kPassPressure * c.pressure;

    const float lane = laneClearance(c, mate.position);
    if (lane < tune::kPassLaneClear) {
        chance *= 1.0f - tune::kPassLanePenalty * (1.0f - lane / tune::kPassLaneClear);
    }
    chance *= markedFactor(c, mate.position);
    if (mate.position.x * c.sign > c.offside + tune::kPassOffsideMargin) {
        chance *= tune::kPassOffside;
    }
    if (through) {
        chance *= tune::kThroughChanceFactor;
    }
    return std::clamp(chance, tune::kPassMinChance, tune::kPassMaxChance);
}

void addPassing(const Context& c, std::vector<Candidate>& out)
{
    const std::size_t base = firstIndex(c.side);
    const float sight = tune::kPassVisionRange + tune::kPassVisionBonus * gameplay::skill(c.carrier.profile.attributes.vision);
    const float range = std::min(sight, tune::kPassMaxDistance);
    const float here = threatAt(c.side, c.carrier.position);

    for (std::size_t i = 1; i < kPlayersPerSide; ++i) {
        const Player& mate = c.world.players()[base + i];
        if (mate.id == c.carrier.id) {
            continue;
        }
        const float dist = physics::distance(c.carrier.position, mate.position);
        if (dist < tune::kPassMinDistance || dist > range) {
            continue;
        }

        const bool across = std::abs(mate.position.y - c.carrier.position.y) >= tune::kSwitchMinLateral && mate.position.y * c.carrier.position.y < 0.0f;
        const float advance = (mate.position.x - c.carrier.position.x) * c.sign;

        ActionRequest request;
        request.kind = ActionKind::Pass;
        request.target = mate.id;
        request.aim = mate.position;
        request.power = gameplay::passPowerForDistance(dist);
        const float chance = passChance(c, mate, dist, false);
        const float lift = threatAt(c.side, mate.position) - here + (across ? tune::kSwitchBonus : 0.0f);
        out.push_back(scored(across ? CandidateKind::Switch : CandidateKind::Pass, request, mate.position, chance, chance * lift,
            (1.0f - chance) * lossCost(c.side, mate.position)));

        if (advance >= tune::kThroughMinAdvance) {
            ActionRequest through = request;
            through.kind = ActionKind::Through;
            through.power = tune::kThroughPower;
            through.aim = gameplay::leadPoint(c.carrier.position, mate, gameplay::passSpeed(through.power));
            const float throughChance = passChance(c, mate, dist, true);
            const float throughLift = threatAt(c.side, through.aim) - here;
            out.push_back(scored(CandidateKind::Through, through, through.aim, throughChance, throughChance * throughLift,
                (1.0f - throughChance) * lossCost(c.side, through.aim)));
        }
    }
}

void addCrosses(const Context& c, std::vector<Candidate>& out)
{
    const Vec2 pos = c.carrier.position;
    const float toLine = physics::pitch::kHalfLength - pos.x * c.sign;
    if (std::abs(pos.y) < tune::kCrossMinWidth || toLine > tune::kCrossMaxGoalLineDistance) {
        return;
    }
    const std::size_t base = firstIndex(c.side);
    const float crossing = lerp(tune::kCrossCrossingLow, tune::kCrossCrossingHigh, gameplay::skill(c.carrier.profile.attributes.crossing));
    const float here = threatAt(c.side, pos);

    for (std::size_t i = 1; i < kPlayersPerSide; ++i) {
        const Player& mate = c.world.players()[base + i];
        if (mate.id == c.carrier.id || physics::distance(mate.position, c.goal) > tune::kCrossBoxReach) {
            continue;
        }
        const float dist = physics::distance(pos, mate.position);
        if (dist < tune::kCrossMinDistance || dist > tune::kCrossMaxDistance) {
            continue;
        }
        const float chance = std::clamp(tune::kCrossBaseChance * crossing * (1.0f - tune::kPassPressure * c.pressure) * markedFactor(c, mate.position),
            tune::kPassMinChance, tune::kPassMaxChance);

        ActionRequest request;
        request.kind = ActionKind::Cross;
        request.target = mate.id;
        request.aim = mate.position;
        const float lift = threatAt(c.side, mate.position) - here + tune::kCrossBoxBonus;
        out.push_back(scored(CandidateKind::Cross, request, mate.position, chance, chance * lift, (1.0f - chance) * lossCost(c.side, mate.position)));
    }

    const Vec2 spot = physics::pitch::penaltySpot(physics::opposite(c.side));
    const float mixerChance = std::clamp(tune::kCrossMixerChance * crossing * (1.0f - tune::kPassPressure * c.pressure), tune::kPassMinChance, tune::kPassMaxChance);
    ActionRequest mixer;
    mixer.kind = ActionKind::Cross;
    mixer.aim = spot;
    const float mixerLift = threatAt(c.side, spot) - here + tune::kCrossBoxBonus;
    out.push_back(scored(CandidateKind::Cross, mixer, spot, mixerChance, mixerChance * mixerLift, (1.0f - mixerChance) * lossCost(c.side, spot)));
}

}

std::string_view candidateName(CandidateKind kind)
{
    switch (kind) {
    case CandidateKind::Shoot: return "shoot";
    case CandidateKind::Dribble: return "dribble";
    case CandidateKind::Pass: return "pass";
    case CandidateKind::Through: return "through";
    case CandidateKind::Cross: return "cross";
    case CandidateKind::Switch: return "switch";
    case CandidateKind::Hold: return "hold";
    }
    return "hold";
}

float threatAt(Side attacking, Vec2 point)
{
    const Vec2 goal = physics::pitch::goalCentre(physics::opposite(attacking));
    const float ratio = physics::distance(point, goal) / tune::kThreatScale;
    const float advance = clamp01((point.x * sideSign(attacking) + physics::pitch::kHalfLength) / physics::pitch::kLength);
    return tune::kThreatGoalWeight / (1.0f + ratio * ratio) + tune::kThreatAdvanceWeight * advance;
}

float shotChance(const gameplay::World& world, const Player& shooter)
{
    return shotChanceFor(contextFor(world, shooter));
}

Decision evaluate(const gameplay::World& world, std::size_t carrier, float riskAversion)
{
    const Context c = contextFor(world, world.players()[carrier]);

    Decision decision;
    decision.tick = world.tick();
    decision.player = c.carrier.id;
    addShot(c, decision.candidates);
    decision.candidates.push_back(dribbleCandidate(c));
    addPassing(c, decision.candidates);
    addCrosses(c, decision.candidates);
    decision.candidates.push_back(holdCandidate(c));

    if (riskAversion != tune::kRiskAversion) {
        const float rescale = riskAversion / tune::kRiskAversion;
        for (Candidate& cand : decision.candidates) {
            cand.risk *= rescale;
            cand.utility = cand.gain - cand.risk;
        }
    }

    for (std::size_t i = 1; i < decision.candidates.size(); ++i) {
        if (decision.candidates[i].utility > decision.candidates[decision.chosen].utility) {
            decision.chosen = i;
        }
    }
    return decision;
}

std::uint32_t looseBallChaser(const gameplay::World& world, Side side)
{
    if (world.possessor() != kNoPlayer || world.ballDead()) {
        return kNoPlayer;
    }
    const Vec2 ball = physics::xy(world.ball().position);
    const std::size_t base = firstIndex(side);
    std::uint32_t best = kNoPlayer;
    float bestDist = 1e9f;
    for (std::size_t i = 1; i < kPlayersPerSide; ++i) {
        const float d = physics::distance(world.players()[base + i].position, ball);
        if (d < bestDist) {
            bestDist = d;
            best = static_cast<std::uint32_t>(base + i);
        }
    }
    return best;
}

CarrierPlan Decider::plan(const gameplay::World& world, Side side)
{
    CarrierPlan plan;
    const std::uint32_t holder = world.possessor();
    const std::size_t base = firstIndex(side);
    if (holder == kNoPlayer || holder <= base || holder >= base + kPlayersPerSide) {
        carrier_ = kNoPlayer;
        steering_ = false;
        return plan;
    }

    const Player& player = world.players()[holder];
    if (holder != carrier_) {
        carrier_ = holder;
        nextTick_ = world.tick() + tune::kFirstDecisionDelay + style_.reactionTicks;
        steering_ = false;
    }
    plan.player = holder;

    if (world.tick() >= nextTick_ && player.cooldown == 0) {
        Decision decision = evaluate(world, holder, style_.riskAversion);
        if (style_.noise > 0.0f) {
            float bestScore = 0.0f;
            for (std::size_t i = 0; i < decision.candidates.size(); ++i) {
                const float score = decision.candidates[i].utility + (rng_.nextFloat() * 2.0f - 1.0f) * style_.noise;
                if (i == 0 || score > bestScore) {
                    bestScore = score;
                    decision.chosen = i;
                }
            }
        }
        const Candidate& best = decision.best();
        const int saving = static_cast<int>(tune::kDecisionVisionSaving * gameplay::skill(player.profile.attributes.vision));
        nextTick_ = world.tick() + static_cast<std::uint64_t>(tune::kDecisionInterval - saving + style_.reactionTicks);
        steering_ = best.kind == CandidateKind::Dribble || best.kind == CandidateKind::Hold;
        destination_ = best.destination;
        plan.request = best.request;

        ++stats_.decisions;
        ++stats_.chosen[static_cast<std::size_t>(best.kind)];
        log_.push_back(std::move(decision));
        if (log_.size() > kLogCapacity) {
            log_.pop_front();
        }
    }

    plan.steer = steering_;
    plan.destination = destination_;
    return plan;
}

}

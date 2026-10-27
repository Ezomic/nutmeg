#include "nutmeg/gameplay/world.h"

#include "nutmeg/controls/intent.h"
#include "nutmeg/core/dmath.h"
#include "nutmeg/core/hash.h"
#include "nutmeg/gameplay/locomotion.h"
#include "nutmeg/gameplay/setpiece.h"

#include <algorithm>

namespace nutmeg::gameplay {

namespace {

using physics::Side;
using physics::Vec2;

// 4-4-2 for Home as {x, y} on the Home half; Away is mirrored through the centre spot.
constexpr std::array<Vec2, kPlayersPerSide> kFormation = {{
    {-50.0f, 0.0f},
    {-35.0f, -24.0f},
    {-35.0f, -8.0f},
    {-35.0f, 8.0f},
    {-35.0f, 24.0f},
    {-20.0f, -24.0f},
    {-20.0f, -8.0f},
    {-20.0f, 8.0f},
    {-20.0f, 24.0f},
    {-6.0f, -5.0f},
    {-6.0f, 5.0f},
}};

Vec2 intentOf(const InputFrame& input, Side side)
{
    const float sign = side == Side::Home ? 1.0f : -1.0f;
    return {input.moveY * sign, -input.moveX * sign};
}

void separate(std::array<Player, kPlayerCount>& players)
{
    constexpr float minDistance = 2.0f * locomotion::kBodyRadius;
    for (int pass = 0; pass < locomotion::kSeparationPasses; ++pass) {
        for (std::size_t i = 0; i < kPlayerCount; ++i) {
            for (std::size_t j = i + 1; j < kPlayerCount; ++j) {
                const Vec2 d = players[j].position - players[i].position;
                const float dist = physics::length(d);
                if (dist >= minDistance) {
                    continue;
                }
                const Vec2 normal = dist > 1e-6f ? d * (1.0f / dist) : Vec2{1.0f, 0.0f};
                const Vec2 push = normal * ((minDistance - dist) * 0.5f);
                players[i].position = players[i].position - push;
                players[j].position = players[j].position + push;
            }
        }
    }
}

void clampToPitch(Player& p)
{
    constexpr float maxX = physics::pitch::kHalfLength + locomotion::kPitchMargin;
    constexpr float maxY = physics::pitch::kHalfWidth + locomotion::kPitchMargin;
    if (p.position.x > maxX) {
        p.position.x = maxX;
        p.velocity.x = p.velocity.x > 0.0f ? 0.0f : p.velocity.x;
    } else if (p.position.x < -maxX) {
        p.position.x = -maxX;
        p.velocity.x = p.velocity.x < 0.0f ? 0.0f : p.velocity.x;
    }
    if (p.position.y > maxY) {
        p.position.y = maxY;
        p.velocity.y = p.velocity.y > 0.0f ? 0.0f : p.velocity.y;
    } else if (p.position.y < -maxY) {
        p.position.y = -maxY;
        p.velocity.y = p.velocity.y < 0.0f ? 0.0f : p.velocity.y;
    }
}

namespace tune = possession;

Vec2 facingOf(const Player& p) { return {dmath::cos(p.facing), dmath::sin(p.facing)}; }

Vec2 unitOr(Vec2 v, Vec2 fallback, float minLength)
{
    const float len = physics::length(v);
    return len > minLength ? v * (1.0f / len) : fallback;
}

std::uint32_t assistTarget(const std::array<Player, kPlayerCount>& players, const Player& from, Vec2 direction)
{
    std::uint32_t best = kNoPlayer;
    float bestScore = -1e9f;
    for (const Player& mate : players) {
        if (mate.side != from.side || mate.id == from.id) {
            continue;
        }
        const Vec2 d = mate.position - from.position;
        const float dist = physics::length(d);
        if (dist < tune::kAssistMinDistance || dist > tune::kAssistMaxDistance) {
            continue;
        }
        const float cosine = physics::dot(d, direction) / dist;
        if (cosine < tune::kAssistCosine) {
            continue;
        }
        const float score = cosine - dist * tune::kAssistDistanceWeight;
        if (score > bestScore) {
            bestScore = score;
            best = mate.id;
        }
    }
    return best;
}

ActionRequest requestFromIntent(const std::array<Player, kPlayerCount>& players, const Player& p, const controls::Intent& intent)
{
    const Vec2 stick{intent.move.x, intent.move.y};
    const Vec2 direction = unitOr(stick, facingOf(p), locomotion::kFacingDeadzone);
    ActionRequest request;

    const auto passLike = [&](ActionKind kind, float power) {
        request.kind = kind;
        request.power = power;
        request.target = assistTarget(players, p, direction);
        request.aim = p.position + direction * 10.0f;
    };

    if (intent.shoot.fire) {
        const Side defending = physics::opposite(p.side);
        const float lateral = std::clamp(stick.y, -1.0f, 1.0f);
        request.kind = ActionKind::Shoot;
        request.power = intent.shoot.power;
        request.aim = {physics::pitch::goalLineX(defending), lateral * physics::pitch::kHalfGoalWidth * tune::kShotAimFraction};
    } else if (intent.pass.fire) {
        passLike(ActionKind::Pass, intent.pass.power);
    } else if (intent.through.fire) {
        passLike(ActionKind::Through, intent.through.power);
    } else if (intent.tackle) {
        request.kind = ActionKind::Tackle;
    }
    return request;
}

std::array<schema::PlayerProfile, kPlayerCount> defaultProfiles()
{
    std::array<schema::PlayerProfile, kPlayerCount> profiles;
    for (std::size_t i = 0; i < kPlayerCount; ++i) {
        profiles[i].id = static_cast<std::uint32_t>(i);
        profiles[i].position = i % kPlayersPerSide == 0 ? schema::Position::Goalkeeper : schema::Position::Midfielder;
    }
    return profiles;
}

}

World::World(std::uint64_t seed)
    : World(defaultProfiles(), seed)
{
}

World::World(const std::array<schema::PlayerProfile, kPlayerCount>& profiles, std::uint64_t seed)
    : rng_(Rng(seed).derive("possession"))
    , referee_(Rng(seed).derive("referee"), kPlayerCount)
    , keepers_(Rng(seed).derive("keeper"))
{
    for (std::size_t i = 0; i < kPlayerCount; ++i) {
        Player& p = players_[i];
        const bool home = i < kPlayersPerSide;
        const Vec2 slot = kFormation[i % kPlayersPerSide];
        p.id = static_cast<std::uint32_t>(i);
        p.side = home ? Side::Home : Side::Away;
        p.position = home ? slot : Vec2{-slot.x, -slot.y};
        p.facing = home ? 0.0f : dmath::kPi;
        p.profile = profiles[i];
    }
}

void World::step(const PlayerInputs& inputs)
{
    step(inputs, PlayerActions{});
}

void World::step(const PlayerInputs& inputs, const PlayerActions& actions)
{
    events_.clear();
    PlayerActions requests;
    collectRequests(inputs, actions, requests);
    keepers_.plan(keeperSituation(), inputs, requests);

    for (std::size_t i = 0; i < kPlayerCount; ++i) {
        Player& p = players_[i];
        if (p.sentOff) {
            continue;
        }
        if (p.cooldown > 0) {
            --p.cooldown;
        }
        const KeeperDrive* keeperDrive = keepers_.drive(i);
        const bool sprint = keeperDrive ? keeperDrive->sprint : (inputs[i].buttons & InputFrame::Sprint) != 0;
        locomotion::step(p, keeperDrive ? keeperDrive->intent : intentOf(inputs[i], p.side), sprint);
    }
    separate(players_);
    for (Player& p : players_) {
        if (!p.sentOff) {
            clampToPitch(p);
        }
    }
    if (restart_) {
        setpiece::keepClear(players_.data(), kPlayerCount, *restart_);
    }

    if (possessor_ != kNoPlayer) {
        attachBall(players_[possessor_]);
    }
    if (restart_) {
        ball_.position = {restart_->spot.x, restart_->spot.y, physics::ball::kRadius};
        ball_.velocity = {};
        ball_.spin = {};
    }
    if (!ballDead_) {
        resolveActions(requests);
        if (possessor_ == kNoPlayer && !restart_) {
            resolveLooseBall();
        }
    }
    keepers_.resolve(keeperSituation());
    if (restart_ && physics::length(ball_.velocity) > 0.0f) {
        restart_.reset();
    }
    ball_.step();
    checkBallStatus();
    review();
    if (pending_ && tick_ >= pendingAt_) {
        applyRestart(*pending_);
    }
    ++tick_;
}

void World::collectRequests(const PlayerInputs& inputs, const PlayerActions& actions, PlayerActions& requests)
{
    for (std::size_t i = 0; i < kPlayerCount; ++i) {
        controls_[i].update(inputs[i]);
        if (actions[i].kind != ActionKind::None) {
            requests[i] = actions[i];
            continue;
        }
        controls::IntentConfig config;
        config.attackDirection = players_[i].side == Side::Home ? 1.0f : -1.0f;
        requests[i] = requestFromIntent(players_, players_[i], controls::desiredIntent(inputs[i], controls_[i], config));
    }
}

void World::emit(MatchEvent event)
{
    event.tick = tick_;
    const bool touch = event.kind == EventKind::Touch || event.kind == EventKind::Pass || event.kind == EventKind::Shot;
    if (touch && event.player != indirectTaker_) {
        indirectTaker_ = kNoPlayer;
    }
    events_.push_back(event);
}

KeeperSituation World::keeperSituation()
{
    return {players_, ball_, possessor_, lastToucher_, ballDead_, tick_, events_};
}

void World::review()
{
    const Situation situation{players_.data(), kPlayerCount, possessor_, tick_, offsideExempt_};
    offsideExempt_ = false;
    const std::optional<Restart> restart = referee_.review(situation, events_);
    for (const MatchEvent& event : events_) {
        if (event.kind == EventKind::RedCard) {
            sendOff(players_[event.player]);
        }
    }
    if (restart) {
        applyRestart(*restart);
    } else {
        scheduleRestart();
    }
}

void World::scheduleRestart()
{
    for (const MatchEvent& event : events_) {
        if (const std::optional<Restart> next = setpiece::restartAfter(event, physics::xy(ball_.position))) {
            pending_ = next;
            pendingAt_ = tick_ + setpiece::kDeadBallTicks;
        }
    }
}

void World::kickoff(Side side)
{
    applyRestart({RestartKind::Kickoff, side, {0.0f, 0.0f}});
}

void World::restartPlay(Restart restart)
{
    applyRestart(restart);
}

void World::applyRestart(const Restart& restart)
{
    restart_ = restart;
    pending_.reset();
    restartSince_ = tick_;
    indirectTaker_ = kNoPlayer;
    setpiece::arrange(players_.data(), kPlayerCount, *restart_);
    ball_.position = {restart.spot.x, restart.spot.y, physics::ball::kRadius};
    ball_.velocity = {};
    ball_.spin = {};
    possessor_ = kNoPlayer;
    ballDead_ = false;
}

void World::sendOff(Player& player)
{
    constexpr float touchlineGap = 6.0f;
    const float y = physics::pitch::kHalfWidth + touchlineGap;
    player.sentOff = true;
    player.position = {-physics::pitch::kHalfLength + 2.0f * static_cast<float>(player.id % kPlayersPerSide), player.side == Side::Home ? -y : y};
    player.velocity = {};
    if (possessor_ == player.id) {
        possessor_ = kNoPlayer;
    }
}

bool World::barredByRestart(const Player& player) const
{
    return restart_ && player.side != restart_->side;
}

void World::attachBall(const Player& carrier)
{
    const float speed = physics::length(carrier.velocity);
    const Vec2 direction = unitOr(carrier.velocity, facingOf(carrier), tune::kCarryMinSpeed);
    const float tighten = 1.0f - tune::kCarryDribbleTighten * skill(carrier.profile.attributes.dribbling);
    const float distance = tune::kCarryBase + tune::kCarrySpeedLead * speed * tighten;
    ball_.position = {carrier.position.x + direction.x * distance, carrier.position.y + direction.y * distance, physics::ball::kRadius};
    ball_.velocity = {carrier.velocity.x, carrier.velocity.y, 0.0f};
    ball_.spin = {};
}

void World::resolveActions(const PlayerActions& requests)
{
    for (std::size_t i = 0; i < kPlayerCount; ++i) {
        const ActionRequest& request = requests[i];
        if (request.kind == ActionKind::Tackle) {
            tackle(i);
        } else if (request.kind != ActionKind::None) {
            kick(i, request);
        }
    }
}

void World::kick(std::size_t index, const ActionRequest& rawRequest)
{
    Player& p = players_[index];
    const bool inReach = physics::distance(p.position, physics::xy(ball_.position)) <= tune::kKickReach && ball_.position.z <= tune::kKickHeight;
    if (p.cooldown > 0 || !inReach || barredByRestart(p)) {
        return;
    }

    const ActionRequest request = restart_ ? setpiece::shapeRequest(*restart_, rawRequest) : rawRequest;
    const float pressure = pressureOn(p, players_.data(), players_.size());
    const bool hasTarget = request.target < kPlayerCount;
    Kick result;
    MatchEvent event;
    event.player = p.id;
    event.side = p.side;
    event.action = request.kind;

    switch (request.kind) {
    case ActionKind::Pass:
    case ActionKind::Through: {
        const bool through = request.kind == ActionKind::Through;
        Vec2 aim = request.aim;
        if (hasTarget) {
            const Player& receiver = players_[request.target];
            aim = through ? leadPoint(p.position, receiver, passSpeed(request.power)) : receiver.position;
        }
        result = passKick(p, aim, request.power, pressure, through, rng_);
        event.kind = EventKind::Pass;
        break;
    }
    case ActionKind::Cross: {
        const Vec2 aim = hasTarget ? players_[request.target].position : request.aim;
        result = crossKick(p, aim, pressure, rng_);
        event.kind = EventKind::Pass;
        break;
    }
    case ActionKind::Shoot:
        result = shotKick(p, request.aim, request.power, pressure, request.lofted, rng_);
        event.kind = EventKind::Shot;
        break;
    default:
        return;
    }

    if (restart_) {
        setpiece::shapeKick(*restart_, result, ball_.position);
        offsideExempt_ = setpiece::exemptFromOffside(restart_->kind);
        indirectTaker_ = setpiece::scoresIndirectly(restart_->kind) ? p.id : kNoPlayer;
    }
    event.other = hasTarget ? request.target : kNoPlayer;
    event.value = physics::length(result.velocity);
    ball_.velocity = result.velocity;
    ball_.spin = result.spin;
    possessor_ = kNoPlayer;
    lastToucher_ = p.id;
    p.cooldown = tune::kKickCooldown;
    emit(event);
}

void World::tackle(std::size_t index)
{
    Player& tackler = players_[index];
    if (possessor_ == kNoPlayer || tackler.cooldown > 0) {
        return;
    }
    Player& carrier = players_[possessor_];
    if (keepers_.protects(carrier) || carrier.side == tackler.side || physics::distance(tackler.position, carrier.position) > tune::kTackleReach) {
        return;
    }

    const bool contested = rng_.nextFloat() < tackleSuccessChance(tackler, carrier);
    const bool foul = rng_.nextFloat() < foulChance(tackler, contested);
    const bool won = contested && !foul;

    MatchEvent event;
    event.kind = EventKind::Tackle;
    event.player = tackler.id;
    event.other = carrier.id;
    event.side = tackler.side;
    event.action = ActionKind::Tackle;
    event.value = won ? 1.0f : 0.0f;
    emit(event);
    if (foul) {
        event.kind = EventKind::FoulCandidate;
        event.value = 0.0f;
        emit(event);
    }

    if (won) {
        const Vec2 away = unitOr(tackler.position - carrier.position, facingOf(tackler), 1e-4f);
        ball_.velocity = {away.x * tune::kTackleKnock, away.y * tune::kTackleKnock, 0.0f};
        possessor_ = kNoPlayer;
        lastToucher_ = tackler.id;
        carrier.cooldown = tune::kTackledCooldown;
    } else {
        tackler.cooldown = tune::kTackleFailCooldown;
    }
}

void World::resolveLooseBall()
{
    const Vec2 ball = physics::xy(ball_.position);

    std::uint32_t winner = kNoPlayer;
    float bestScore = -1e9f;
    if (ball_.position.z <= tune::kControlHeight) {
        for (const Player& p : players_) {
            const float d = physics::distance(p.position, ball);
            if (p.cooldown > 0 || d > tune::kControlRadius || barredByRestart(p)) {
                continue;
            }
            const auto& a = p.profile.attributes;
            const float ability = 0.5f * skill(a.firstTouch) + 0.2f * skill(a.dribbling) + 0.3f * skill(a.strength);
            const float score = ability + rng_.nextFloat() * tune::kContestNoise - d * tune::kContestDistanceWeight;
            if (score > bestScore) {
                bestScore = score;
                winner = p.id;
            }
        }
    }

    if (winner == kNoPlayer) {
        for (Player& p : players_) {
            if (p.cooldown == 0 && !barredByRestart(p) && ball_.position.z <= tune::kDeflectHeight && physics::distance(p.position, ball) < tune::kBodyContactRadius) {
                deflect(p);
            }
        }
        return;
    }

    Player& p = players_[winner];
    const physics::Vec3 relative = ball_.velocity - physics::Vec3{p.velocity.x, p.velocity.y, 0.0f};
    const float trapLimit = tune::kTrapSpeedBase + tune::kTrapSpeedSkill * skill(p.profile.attributes.firstTouch);
    const float quality = 1.0f - physics::length(relative) / trapLimit;

    MatchEvent event;
    event.kind = EventKind::Touch;
    event.player = p.id;
    event.side = p.side;
    lastToucher_ = p.id;

    if (quality >= 0.0f) {
        possessor_ = p.id;
        attachBall(p);
        event.value = quality;
        emit(event);
        return;
    }

    const float sx = rng_.nextFloat() * 2.0f - 1.0f;
    const float sy = rng_.nextFloat() * 2.0f - 1.0f;
    const float scatter = tune::kHeavyTouchScatter * std::min(-quality, 1.0f) * physics::length(relative);
    Vec2 rel = {relative.x * tune::kHeavyTouchKeep + sx * scatter, relative.y * tune::kHeavyTouchKeep + sy * scatter};
    const Vec2 outward = unitOr(ball - p.position, facingOf(p), 1e-4f);
    const float into = physics::dot(rel, outward);
    if (into < 0.0f) {
        rel = rel - outward * into;
    }
    ball_.velocity = {p.velocity.x + rel.x, p.velocity.y + rel.y, relative.z * tune::kHeavyTouchKeep};
    p.cooldown = tune::kHeavyTouchCooldown;
    emit(event);
}

void World::deflect(Player& player)
{
    const Vec2 ball = physics::xy(ball_.position);
    const Vec2 normal = unitOr(ball - player.position, facingOf(player), 1e-4f);
    Vec2 rel = physics::xy(ball_.velocity) - player.velocity;
    const float along = physics::dot(rel, normal);
    if (along < 0.0f) {
        rel = rel - normal * ((1.0f + tune::kDeflectRestitution) * along);
        ball_.velocity.x = player.velocity.x + rel.x;
        ball_.velocity.y = player.velocity.y + rel.y;
    }
    const Vec2 pushed = player.position + normal * tune::kBodyContactRadius;
    ball_.position.x = pushed.x;
    ball_.position.y = pushed.y;
    lastToucher_ = player.id;

    MatchEvent event;
    event.kind = EventKind::Touch;
    event.player = player.id;
    event.side = player.side;
    emit(event);
}

void World::checkBallStatus()
{
    const physics::BallStatus status = ball_.status();
    if (ballDead_) {
        ballDead_ = pending_ || status.status != physics::PlayStatus::InPlay;
        return;
    }
    if (status.status == physics::PlayStatus::InPlay) {
        return;
    }

    ballDead_ = true;
    possessor_ = kNoPlayer;
    MatchEvent event;
    event.player = lastToucher_;
    event.line = status.line;
    if (status.status == physics::PlayStatus::Goal && indirectTaker_ == kNoPlayer) {
        event.kind = EventKind::Goal;
        event.side = status.scoredFor;
    } else {
        event.kind = EventKind::OutOfPlay;
        event.side = lastToucher_ < kPlayerCount ? players_[lastToucher_].side : Side::Home;
    }
    emit(event);
}

std::uint64_t World::hash() const
{
    std::uint64_t h = mix64(tick_);
    for (const Player& p : players_) {
        h = hashCombine(h, p.hash());
    }
    h = hashCombine(h, ball_.hash());
    h = hashCombine(h, mix64(possessor_));
    h = hashCombine(h, mix64(lastToucher_));
    h = hashCombine(h, mix64(ballDead_ ? 1u : 0u));
    h = hashCombine(h, mix64(rng_.state()));
    h = hashCombine(h, keepers_.hash());
    h = hashCombine(h, referee_.hash());
    h = hashCombine(h, mix64(restart_ ? 1u + static_cast<std::uint64_t>(restart_->kind) + (static_cast<std::uint64_t>(restart_->taker) << 8) : 0u));
    h = hashCombine(h, mix64(pending_ ? 1u + static_cast<std::uint64_t>(pending_->kind) + (pendingAt_ << 8) : 0u));
    h = hashCombine(h, mix64(indirectTaker_));
    h = hashCombine(h, mix64(restartSince_));
    for (const controls::ControlState& c : controls_) {
        for (const InputFrame::Button b : {InputFrame::Pass, InputFrame::Through, InputFrame::Shoot, InputFrame::Sprint, InputFrame::Tackle, InputFrame::Switch, InputFrame::Pause}) {
            h = hashCombine(h, mix64(static_cast<std::uint64_t>(c.heldTicks(b))));
        }
    }
    return h;
}

}

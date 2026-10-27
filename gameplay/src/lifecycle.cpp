#include "nutmeg/gameplay/lifecycle.h"

#include "nutmeg/core/hash.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace nutmeg::gameplay {

namespace {

using physics::Side;
using physics::Vec2;

constexpr std::size_t index(Team team) { return static_cast<std::size_t>(team); }

std::vector<schema::PlayerProfile> makeBench(int size, std::uint32_t firstId)
{
    std::vector<schema::PlayerProfile> bench(static_cast<std::size_t>(size));
    for (std::size_t i = 0; i < bench.size(); ++i) {
        bench[i].id = firstId + static_cast<std::uint32_t>(i);
        bench[i].position = i == 0 ? schema::Position::Goalkeeper : schema::Position::Midfielder;
    }
    return bench;
}

bool isKeeperSlot(std::uint32_t slot) { return slot % kPlayersPerSide == 0; }

std::uint32_t nearestTeammate(const World& world, const Player& taker)
{
    std::uint32_t best = kNoPlayer;
    float bestDistance = std::numeric_limits<float>::max();
    for (const Player& mate : world.players()) {
        if (mate.side != taker.side || mate.id == taker.id || mate.sentOff) {
            continue;
        }
        const float d = physics::distance(mate.position, taker.position);
        if (d < bestDistance) {
            bestDistance = d;
            best = mate.id;
        }
    }
    return best;
}

std::uint64_t ticksOf(int minutes, int ticksPerMinute)
{
    return static_cast<std::uint64_t>(minutes) * static_cast<std::uint64_t>(ticksPerMinute);
}

}

Lifecycle::Lifecycle(Rng rng, LifecycleConfig config)
    : rng_(rng)
    , config_(config)
{
    const auto firstId = static_cast<std::uint32_t>(kPlayerCount);
    bench_[index(Team::A)] = makeBench(config_.benchSize, firstId);
    bench_[index(Team::B)] = makeBench(config_.benchSize, firstId + static_cast<std::uint32_t>(config_.benchSize));
}

Side Lifecycle::sideOf(Team team) const
{
    return (team == Team::A) != swapped_ ? Side::Home : Side::Away;
}

Team Lifecycle::teamOf(Side side) const
{
    return (side == Side::Home) != swapped_ ? Team::A : Team::B;
}

int Lifecycle::minute() const
{
    const int played = std::min(config_.halfMinutes, static_cast<int>(halfTicks_ / static_cast<std::uint64_t>(config_.ticksPerMinute)));
    return (phase_ == Phase::SecondHalf || phase_ == Phase::FullTime ? config_.halfMinutes : 0) + played;
}

void Lifecycle::setPhase(Phase phase)
{
    phase_ = phase;
    LifecycleEvent event;
    event.kind = LifecycleEventKind::PhaseChange;
    event.phase = phase;
    emit(event);
}

void Lifecycle::emit(LifecycleEvent event)
{
    event.tick = tick_;
    log_.push_back(event);
}

PlayerActions Lifecycle::before(World& world)
{
    if (phase_ == Phase::PreMatch) {
        startHalf(world, Team::A);
        setPhase(Phase::FirstHalf);
    } else if (phase_ == Phase::HalfTime) {
        if (intervalLeft_ > 0) {
            --intervalLeft_;
        }
        if (intervalLeft_ == 0) {
            swapEnds(world);
            startHalf(world, Team::B);
            setPhase(Phase::SecondHalf);
        }
    }
    if (!playing()) {
        return {};
    }
    if (world.restart() && world.restartSince() != handledRestart_) {
        handledRestart_ = world.restartSince();
        resolveInjuries(world);
        tryTactical(world, config_.stoppageSubstitutionChance);
    }
    return takeRestart(world);
}

void Lifecycle::after(World& world)
{
    ++tick_;
    if (!playing()) {
        return;
    }
    for (const MatchEvent& event : world.events()) {
        if (event.kind == EventKind::Goal) {
            ++goals_[index(teamOf(event.side))];
        } else if (event.kind == EventKind::Foul && event.other < kPlayerCount && rng_.nextFloat() < config_.foulInjuryChance) {
            injure(world, event.other);
        }
    }
    rollInjuries(world);
    tallyStoppage(world);
    ++halfTicks_;
    endHalf(world);
}

void Lifecycle::startHalf(World& world, Team kickingOff)
{
    halfTicks_ = 0;
    stoppedTicks_ = 0;
    creditTicks_ = 0;
    addedMinutes_ = 0;
    addedDecided_ = false;
    handledRestart_ = ~0ull;
    world.kickoff(sideOf(kickingOff));
}

void Lifecycle::swapEnds(World& world)
{
    auto& players = world.players();
    for (std::uint32_t i = 0; i < kPlayersPerSide; ++i) {
        const std::uint32_t j = i + static_cast<std::uint32_t>(kPlayersPerSide);
        std::swap(players[i].profile, players[j].profile);
        std::swap(players[i].stamina, players[j].stamina);
        std::swap(players[i].sentOff, players[j].sentOff);
        players[i].cooldown = 0;
        players[j].cooldown = 0;
        world.referee().exchange(i, j);
    }
    swapped_ = !swapped_;
    for (Player& p : players) {
        if (p.sentOff) {
            leavePitch(p);
        }
    }
}

void Lifecycle::leavePitch(Player& player)
{
    constexpr float touchlineGap = 6.0f;
    const float y = physics::pitch::kHalfWidth + touchlineGap;
    player.sentOff = true;
    player.position = {-physics::pitch::kHalfLength + 2.0f * static_cast<float>(player.id % kPlayersPerSide), player.side == Side::Home ? -y : y};
    player.velocity = {};
}

void Lifecycle::tallyStoppage(const World& world)
{
    if (world.ballDead() || world.restart()) {
        ++stoppedTicks_;
    }
}

void Lifecycle::rollInjuries(World& world)
{
    const float perTick = config_.injuryPerPlayerMinute * static_cast<float>(kPlayerCount) / static_cast<float>(config_.ticksPerMinute);
    if (rng_.nextFloat() >= perTick) {
        return;
    }
    injure(world, rng_.range(0, static_cast<std::uint32_t>(kPlayerCount - 1)));
}

void Lifecycle::injure(const World& world, std::uint32_t slot)
{
    const Player& p = world.players()[slot];
    if (p.sentOff || std::find(injured_.begin(), injured_.end(), slot) != injured_.end()) {
        return;
    }
    injured_.push_back(slot);
    LifecycleEvent event;
    event.kind = LifecycleEventKind::Injury;
    event.side = p.side;
    event.player = slot;
    emit(event);
}

void Lifecycle::resolveInjuries(World& world)
{
    const std::uint32_t taker = world.restart() ? world.restart()->taker : kNoPlayer;
    std::vector<std::uint32_t> waiting;
    for (const std::uint32_t slot : injured_) {
        if (slot == taker) {
            waiting.push_back(slot);
            continue;
        }
        if (!substitute(world, slot)) {
            Player& p = world.players()[slot];
            leavePitch(p);
            LifecycleEvent event;
            event.kind = LifecycleEventKind::PlayerLeft;
            event.side = p.side;
            event.player = slot;
            emit(event);
        }
    }
    injured_ = waiting;
}

void Lifecycle::tryTactical(World& world, float chance)
{
    if (minute() < config_.substitutionFromMinute || rng_.nextFloat() >= chance) {
        return;
    }
    const std::uint32_t taker = world.restart() ? world.restart()->taker : kNoPlayer;
    const Side side = sideOf(rng_.nextFloat() < 0.5f ? Team::A : Team::B);
    const std::uint32_t first = side == Side::Home ? 0u : static_cast<std::uint32_t>(kPlayersPerSide);
    const std::uint32_t slot = first + rng_.range(1, static_cast<std::uint32_t>(kPlayersPerSide - 1));
    if (slot != taker) {
        substitute(world, slot);
    }
}

bool Lifecycle::substitute(World& world, std::uint32_t slot)
{
    Player& p = world.players()[slot];
    const Team team = teamOf(p.side);
    auto& bench = bench_[index(team)];
    if (p.sentOff || used_[index(team)] >= config_.maxSubstitutions) {
        return false;
    }
    const bool keeper = isKeeperSlot(slot);
    const auto next = std::find_if(bench.begin(), bench.end(), [&](const schema::PlayerProfile& b) {
        return (b.position == schema::Position::Goalkeeper) == keeper;
    });
    if (next == bench.end()) {
        return false;
    }
    p.profile = *next;
    p.stamina = 1.0f;
    p.cooldown = 0;
    ++used_[index(team)];
    creditTicks_ += static_cast<std::uint64_t>(config_.stoppageMinutesPerChange * static_cast<float>(config_.ticksPerMinute));

    LifecycleEvent event;
    event.kind = LifecycleEventKind::Substitution;
    event.side = p.side;
    event.player = slot;
    event.value = static_cast<float>(next->id);
    emit(event);
    bench.erase(next);
    return true;
}

void Lifecycle::endHalf(World& world)
{
    const std::uint64_t limit = ticksOf(config_.halfMinutes, config_.ticksPerMinute);
    if (halfTicks_ < limit) {
        return;
    }
    if (!addedDecided_) {
        const float credited = config_.stoppageCredit * static_cast<float>(stoppedTicks_) + static_cast<float>(creditTicks_);
        addedMinutes_ = std::min(config_.maxAddedMinutes, static_cast<int>(std::ceil(credited / static_cast<float>(config_.ticksPerMinute))));
        addedDecided_ = true;
        LifecycleEvent event;
        event.kind = LifecycleEventKind::AddedTime;
        event.phase = phase_;
        event.value = static_cast<float>(addedMinutes_);
        emit(event);
    }
    const std::uint64_t end = limit + ticksOf(addedMinutes_, config_.ticksPerMinute);
    const bool stopped = world.ballDead() || world.restart();
    if (halfTicks_ < end || (!stopped && halfTicks_ < end + ticksOf(1, config_.ticksPerMinute))) {
        return;
    }
    if (phase_ == Phase::FirstHalf) {
        resolveInjuries(world);
        tryTactical(world, config_.intervalSubstitutionChance);
        intervalLeft_ = ticksOf(config_.intervalMinutes, config_.ticksPerMinute);
        setPhase(Phase::HalfTime);
    } else {
        setPhase(Phase::FullTime);
    }
}

PlayerActions Lifecycle::takeRestart(const World& world) const
{
    PlayerActions actions{};
    const auto& restart = world.restart();
    if (!restart || restart->taker >= kPlayerCount || world.tick() < world.restartSince() + static_cast<std::uint64_t>(config_.restartDelayTicks)) {
        return actions;
    }
    const Player& taker = world.players()[restart->taker];
    ActionRequest& request = actions[restart->taker];
    if (restart->kind == RestartKind::Penalty) {
        request.kind = ActionKind::Shoot;
        request.aim = {physics::pitch::goalLineX(physics::opposite(restart->side)), 0.0f};
        request.power = 0.8f;
        return actions;
    }
    const std::uint32_t mate = nearestTeammate(world, taker);
    request.kind = ActionKind::Pass;
    request.target = mate;
    request.aim = mate < kPlayerCount ? world.players()[mate].position : Vec2{};
    request.power = 0.5f;
    return actions;
}

std::uint64_t Lifecycle::hash() const
{
    std::uint64_t h = mix64(tick_);
    h = hashCombine(h, mix64(static_cast<std::uint64_t>(phase_)));
    h = hashCombine(h, mix64(halfTicks_));
    h = hashCombine(h, mix64(stoppedTicks_ + (creditTicks_ << 20)));
    h = hashCombine(h, mix64(intervalLeft_));
    h = hashCombine(h, mix64(static_cast<std::uint64_t>(addedMinutes_) | (swapped_ ? 1ull << 8 : 0ull) | (addedDecided_ ? 1ull << 9 : 0ull)));
    h = hashCombine(h, mix64(rng_.state()));
    for (std::size_t t = 0; t < 2; ++t) {
        h = hashCombine(h, mix64(static_cast<std::uint64_t>(goals_[t]) | (static_cast<std::uint64_t>(used_[t]) << 16) | (bench_[t].size() << 32)));
    }
    for (const std::uint32_t slot : injured_) {
        h = hashCombine(h, mix64(slot));
    }
    return hashCombine(h, mix64(log_.size()));
}

}

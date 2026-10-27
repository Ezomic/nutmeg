#include "nutmeg/sim/replay.h"

#include <algorithm>

namespace nutmeg {

using gameplay::EventKind;
using gameplay::MatchEvent;

MatchRecording::MatchRecording(const Simulation& start, InputStream inputs, std::uint64_t keyframeInterval)
    : inputs_(std::move(inputs))
    , interval_(std::max<std::uint64_t>(keyframeInterval, 1))
{
    Simulation sim = start;
    hashes_.reserve(inputs_.size() + 1);
    keyframes_.push_back({0, sim, sim.stateHash()});
    hashes_.push_back(keyframes_.back().hash);
    for (std::uint64_t i = 0; i < inputs_.size(); ++i) {
        sim.tick(inputs_.at(i));
        for (gameplay::MatchEvent event : sim.world().events()) {
            event.tick = i + 1;
            events_.push_back(event);
        }
        hashes_.push_back(sim.stateHash());
        if ((i + 1) % interval_ == 0) {
            keyframes_.push_back({i + 1, sim, hashes_.back()});
        }
    }
}

const Keyframe& MatchRecording::keyframeFor(std::uint64_t index) const
{
    return keyframes_[std::min<std::uint64_t>(index / interval_, keyframes_.size() - 1)];
}

namespace {

physics::Line goalLineAttackedBy(physics::Side side)
{
    return side == physics::Side::Home ? physics::Line::GoalLinePlusX : physics::Line::GoalLineMinusX;
}

bool isDecisive(EventKind kind)
{
    return kind == EventKind::Goal || kind == EventKind::Save || kind == EventKind::OutOfPlay;
}

}

std::vector<Highlight> detectHighlights(const MatchRecording& recording, const HighlightOptions& options)
{
    return detectHighlights(recording.events(), recording.size(), options);
}

std::vector<Highlight> detectHighlights(const std::vector<MatchEvent>& events, std::uint64_t length, const HighlightOptions& options)
{
    std::vector<Highlight> found;
    const auto add = [&](HighlightKind kind, const MatchEvent& event) {
        const std::uint64_t at = std::min(event.tick, length);
        found.push_back({kind, at, at > options.leadTicks ? at - options.leadTicks : 0, std::min(at + options.tailTicks, length)});
    };

    for (std::size_t i = 0; i < events.size(); ++i) {
        const MatchEvent& event = events[i];
        if (event.kind == EventKind::Goal) {
            add(HighlightKind::Goal, event);
        } else if (event.kind == EventKind::Save) {
            add(HighlightKind::Save, event);
        } else if (event.kind == EventKind::Shot && event.player < gameplay::kPlayerCount) {
            const physics::Side shooter = event.player < gameplay::kPlayersPerSide ? physics::Side::Home : physics::Side::Away;
            for (std::size_t j = i + 1; j < events.size() && events[j].tick - event.tick <= options.nearMissTicks; ++j) {
                if (!isDecisive(events[j].kind)) {
                    continue;
                }
                if (events[j].kind == EventKind::OutOfPlay && events[j].line == goalLineAttackedBy(shooter)) {
                    add(HighlightKind::NearMiss, events[j]);
                }
                break;
            }
        }
    }
    return found;
}

ReplayPlayer::ReplayPlayer(const MatchRecording& recording, std::uint64_t begin, std::uint64_t end)
    : recording_(recording)
    , sim_(recording.keyframeFor(begin).sim)
    , index_(recording.keyframeFor(begin).index)
    , end_(std::min(end, recording.size()))
{
    while (index_ < std::min(begin, end_)) {
        step();
    }
}

bool ReplayPlayer::step()
{
    if (done()) {
        return false;
    }
    sim_.tick(recording_.inputs().at(index_));
    ++index_;
    matches_ = matches_ && sim_.stateHash() == recording_.hashAt(index_);
    return true;
}

}

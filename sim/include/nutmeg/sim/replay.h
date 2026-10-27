#pragma once

#include "nutmeg/gameplay/events.h"
#include "nutmeg/sim/input_stream.h"
#include "nutmeg/sim/simulation.h"

#include <cstdint>
#include <vector>

namespace nutmeg {

// Positions in a recording are stream indices: index i is the state after i
// ticks of the stream, so a window [begin, end) covers ticks begin..end-1.

struct Keyframe {
    std::uint64_t index = 0;
    Simulation sim;
    std::uint64_t hash = 0;
};

// A full run re-simulated from its start state and input stream, with the
// state saved every `keyframeInterval` ticks so any window can be reached
// without replaying from the start.
class MatchRecording {
public:
    static constexpr std::uint64_t kDefaultKeyframeInterval = 300;

    MatchRecording(const Simulation& start, InputStream inputs, std::uint64_t keyframeInterval = kDefaultKeyframeInterval);

    const InputStream& inputs() const { return inputs_; }
    std::uint64_t size() const { return inputs_.size(); }
    std::uint64_t keyframeInterval() const { return interval_; }
    const std::vector<Keyframe>& keyframes() const { return keyframes_; }
    // The last keyframe at or before `index`.
    const Keyframe& keyframeFor(std::uint64_t index) const;
    // Every event of the run, in order; `tick` is the world tick.
    const std::vector<gameplay::MatchEvent>& events() const { return events_; }
    // The stream index an event happened at.
    std::uint64_t indexOf(const gameplay::MatchEvent& event) const;
    std::uint64_t baseTick() const { return baseTick_; }
    // The state hash after `index` ticks; index 0 is the start state.
    std::uint64_t hashAt(std::uint64_t index) const { return hashes_[index]; }

private:
    InputStream inputs_;
    std::uint64_t interval_;
    std::uint64_t baseTick_;
    std::vector<Keyframe> keyframes_;
    std::vector<std::uint64_t> hashes_;
    std::vector<gameplay::MatchEvent> events_;
};

enum class HighlightKind : std::uint8_t { Goal, Save, NearMiss };

struct Highlight {
    HighlightKind kind = HighlightKind::Goal;
    // The stream index of the event the window is built around.
    std::uint64_t at = 0;
    std::uint64_t begin = 0;
    std::uint64_t end = 0;
};

struct HighlightOptions {
    std::uint64_t leadTicks = 300;
    std::uint64_t tailTicks = 180;
    // A shot that leaves play over the goal line this soon after is a near miss.
    std::uint64_t nearMissTicks = 120;
};

// Goals, saves and shots that went out over the goal line, in event order.
// `events` carry world ticks; `baseTick` is the world tick of stream index 0
// and `length` the number of ticks in the stream.
std::vector<Highlight> detectHighlights(const std::vector<gameplay::MatchEvent>& events, std::uint64_t baseTick, std::uint64_t length, const HighlightOptions& options = {});
std::vector<Highlight> detectHighlights(const MatchRecording& recording, const HighlightOptions& options = {});

// Re-runs a window of a recording one tick at a time and exposes the world
// after each, for a renderer to draw. Plays instant replay, goal highlights
// and, over the whole run, spectate.
class ReplayPlayer {
public:
    ReplayPlayer(const MatchRecording& recording, std::uint64_t begin, std::uint64_t end);
    ReplayPlayer(const MatchRecording& recording, const Highlight& highlight)
        : ReplayPlayer(recording, highlight.begin, highlight.end)
    {
    }

    bool done() const { return index_ >= end_; }
    // Plays the next tick. False once the window is over.
    bool step();
    // The stream index of the next tick to play; the world is the state at this index.
    std::uint64_t index() const { return index_; }
    const gameplay::World& world() const { return sim_.world(); }
    std::uint64_t stateHash() const { return sim_.stateHash(); }
    // False once any tick played so far differed from the original run.
    bool matches() const { return matches_; }

private:
    const MatchRecording& recording_;
    Simulation sim_;
    std::uint64_t index_;
    std::uint64_t end_;
    bool matches_ = true;
};

}

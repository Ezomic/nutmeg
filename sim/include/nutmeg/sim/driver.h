#pragma once

#include "nutmeg/platform/platform.h"
#include "nutmeg/sim/input_stream.h"
#include "nutmeg/sim/simulation.h"

#include <cstdint>
#include <functional>

namespace nutmeg {

struct DriverOptions {
    // Stop after this many ticks in total, whether live or replayed.
    std::uint64_t maxTicks = 0;
    // Feed the simulation from this stream instead of the platform's input.
    const InputStream* replay = nullptr;
    // Appends every input the simulation consumed.
    InputStream* record = nullptr;
    // Called once per frame before events are pumped, with the next tick index.
    std::function<void(std::uint64_t)> beforeFrame;
};

// Runs the frame loop: pump the platform, turn elapsed time into fixed steps
// and tick the simulation for each. Needs no renderer, so it is the headless
// loop. Returns the number of ticks run; stops early if the platform quits.
std::uint64_t runSimulation(Platform& platform, Simulation& sim, const DriverOptions& options);

}

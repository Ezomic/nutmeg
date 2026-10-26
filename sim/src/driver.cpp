#include "nutmeg/sim/driver.h"

#include "nutmeg/core/fixed_step.h"

#include <algorithm>

namespace nutmeg {

std::uint64_t runSimulation(Platform& platform, Simulation& sim, const DriverOptions& options)
{
    const std::uint64_t limit = options.replay ? std::min(options.maxTicks, options.replay->size()) : options.maxTicks;
    FixedStep step(Simulation::kStepSeconds);
    std::uint64_t ticks = 0;
    double last = platform.now();

    while (ticks < limit && !platform.quitRequested()) {
        if (options.beforeFrame) {
            options.beforeFrame(ticks);
        }
        platform.pumpEvents();
        const double now = platform.now();
        const int steps = step.advance(now - last);
        last = now;

        for (int i = 0; i < steps && ticks < limit; ++i, ++ticks) {
            const InputFrame input = options.replay ? options.replay->at(ticks) : platform.input();
            if (options.record) {
                options.record->push(input);
            }
            sim.tick(input);
        }
    }
    return ticks;
}

}

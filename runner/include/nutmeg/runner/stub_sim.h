#pragma once

#include "nutmeg/runner/match_input.h"

#include <nlohmann/json.hpp>

namespace nutmeg::runner {

// STUB. A seeded score and event generator that stands in for the real
// simulation. It shows the shape of a result and is deterministic from the
// seed, but it plays no football. Replace the body, keep the result shape.
nlohmann::json runStubSimulation(const MatchInput& input);

}

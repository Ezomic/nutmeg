#pragma once

#include <string_view>

namespace nutmeg {

// Replays are only valid on the engine build that recorded them, so files are
// stamped with this and refused when it differs. Bump it with any change that
// alters simulation results.
inline constexpr std::string_view kEngineVersion = "0.1.0";

}

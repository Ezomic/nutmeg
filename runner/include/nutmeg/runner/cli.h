#pragma once

#include <iosfwd>
#include <string>
#include <vector>

namespace nutmeg::runner {

inline constexpr int kExitOk = 0;
inline constexpr int kExitUsage = 1;
inline constexpr int kExitInvalidInput = 2;
inline constexpr int kExitUnsupportedVersion = 3;

int runCli(const std::vector<std::string>& args, std::ostream& out, std::ostream& err);

}

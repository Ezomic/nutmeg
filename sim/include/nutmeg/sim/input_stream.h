#pragma once

#include "nutmeg/core/input.h"
#include "nutmeg/sim/version.h"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace nutmeg {

// The input of every tick of a match, in order. Live play appends to it as it
// goes and replay reads it back, so both feed the simulation the same frames.
class InputStream {
public:
    void push(const InputFrame& frame) { frames_.push_back(frame); }
    const InputFrame& at(std::uint64_t tick) const { return frames_[tick]; }
    std::uint64_t size() const { return frames_.size(); }
    bool operator==(const InputStream&) const = default;

    std::vector<std::uint8_t> toBytes(std::string_view version = kEngineVersion) const;
    // False with `error` set when the bytes are truncated, malformed, carry trailing data, or
    // were written by another engine version. `stream` is left empty on failure.
    static bool fromBytes(std::span<const std::uint8_t> bytes, InputStream& stream, std::string& error, std::string_view version = kEngineVersion);

    bool save(const std::filesystem::path& path) const;
    static bool load(const std::filesystem::path& path, InputStream& stream, std::string& error);

private:
    std::vector<InputFrame> frames_;
};

}

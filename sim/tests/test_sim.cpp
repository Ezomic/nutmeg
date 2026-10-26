#include "nutmeg/platform/null_platform.h"
#include "nutmeg/sim/driver.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

using namespace nutmeg;

namespace {

constexpr std::uint64_t kTenSeconds = 600;

InputFrame scripted(std::uint64_t tick)
{
    InputFrame frame;
    frame.moveX = static_cast<float>(tick % 7) * 0.25f - 0.75f;
    frame.moveY = static_cast<float>(tick % 5) * 0.5f - 1.0f;
    frame.buttons = tick % 11 == 0 ? InputFrame::Pass : (tick % 13 == 0 ? InputFrame::Shoot : 0u);
    return frame;
}

struct Run {
    std::uint64_t ticks = 0;
    std::uint64_t hash = 0;
    InputStream recorded;
};

Run playLive(std::uint64_t seed, std::uint64_t maxTicks)
{
    NullPlatform platform;
    platform.init({});
    Simulation sim(seed);
    Run run;
    DriverOptions options;
    options.maxTicks = maxTicks;
    options.record = &run.recorded;
    options.beforeFrame = [&](std::uint64_t tick) { platform.setInput(scripted(tick)); };
    run.ticks = runSimulation(platform, sim, options);
    run.hash = sim.stateHash();
    return run;
}

std::uint64_t replayHash(const InputStream& stream, std::uint64_t seed)
{
    NullPlatform platform;
    platform.init({});
    Simulation sim(seed);
    DriverOptions options;
    options.maxTicks = stream.size();
    options.replay = &stream;
    REQUIRE(runSimulation(platform, sim, options) == stream.size());
    return sim.stateHash();
}

}

TEST_CASE("the null platform drives a ten second headless run deterministically")
{
    const Run a = playLive(42, kTenSeconds);
    const Run b = playLive(42, kTenSeconds);
    REQUIRE(a.ticks == kTenSeconds);
    REQUIRE(a.recorded.size() == kTenSeconds);
    REQUIRE(a.hash == b.hash);
    REQUIRE(a.recorded == b.recorded);
    REQUIRE(playLive(43, kTenSeconds).hash != a.hash);
}

TEST_CASE("a recorded stream replays to an identical state hash")
{
    const Run live = playLive(7, kTenSeconds);
    REQUIRE(replayHash(live.recorded, 7) == live.hash);
}

TEST_CASE("a saved and reloaded stream replays to the same hash")
{
    const Run live = playLive(9, kTenSeconds);
    const auto path = std::filesystem::temp_directory_path() / "nutmeg_sim_test.nfis";
    REQUIRE(live.recorded.save(path));

    InputStream loaded;
    std::string error;
    REQUIRE(InputStream::load(path, loaded, error));
    std::filesystem::remove(path);
    REQUIRE(loaded == live.recorded);
    REQUIRE(replayHash(loaded, 9) == live.hash);
}

TEST_CASE("a file from another engine version is refused with a clear error")
{
    const Run live = playLive(1, 60);
    const auto bytes = live.recorded.toBytes("0.0.0-other");
    InputStream loaded;
    std::string error;
    REQUIRE_FALSE(InputStream::fromBytes(bytes, loaded, error));
    REQUIRE(error.find("0.0.0-other") != std::string::npos);
    REQUIRE(error.find("engine version") != std::string::npos);
    REQUIRE(loaded.size() == 0);
}

TEST_CASE("truncated, garbage and over-long files are refused")
{
    const Run live = playLive(1, 60);
    const auto bytes = live.recorded.toBytes();
    InputStream loaded;
    std::string error;

    for (std::size_t cut = 0; cut < bytes.size(); ++cut) {
        REQUIRE_FALSE(InputStream::fromBytes(std::span(bytes).first(cut), loaded, error));
        REQUIRE(loaded.size() == 0);
    }

    auto longer = bytes;
    longer.push_back(0);
    REQUIRE_FALSE(InputStream::fromBytes(longer, loaded, error));

    auto garbage = bytes;
    garbage[0] ^= 0xFF;
    REQUIRE_FALSE(InputStream::fromBytes(garbage, loaded, error));

    REQUIRE(InputStream::fromBytes(bytes, loaded, error));
    REQUIRE(loaded == live.recorded);
}

TEST_CASE("a missing file is refused")
{
    InputStream loaded;
    std::string error;
    REQUIRE_FALSE(InputStream::load("does/not/exist.nfis", loaded, error));
    REQUIRE_FALSE(error.empty());
}

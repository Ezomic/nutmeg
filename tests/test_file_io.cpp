// SPDX-License-Identifier: Apache-2.0
// Copyright (c) Nutmeg contributors

#include "nutmeg/core/file_io.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace nutmeg;
namespace fs = std::filesystem;

namespace {

struct TempDir {
    fs::path path = fs::temp_directory_path() / "nutmeg_file_io_test";

    TempDir()
    {
        fs::remove_all(path);
        fs::create_directories(path);
    }
    ~TempDir() { fs::remove_all(path); }

    std::size_t entries() const
    {
        std::size_t count = 0;
        for ([[maybe_unused]] const auto& entry : fs::directory_iterator(path)) {
            ++count;
        }
        return count;
    }
};

void writeRaw(const fs::path& path, const std::string& text) { std::ofstream(path, std::ios::binary) << text; }

}

TEST_CASE("readFileBytes reads a whole file and an empty one")
{
    TempDir dir;
    writeRaw(dir.path / "data.bin", std::string("a\0b", 3));
    writeRaw(dir.path / "empty.bin", "");

    std::vector<std::uint8_t> bytes;
    REQUIRE(readFileBytes(dir.path / "data.bin", bytes));
    REQUIRE(bytes == std::vector<std::uint8_t>{'a', 0, 'b'});

    REQUIRE(readFileBytes(dir.path / "empty.bin", bytes));
    REQUIRE(bytes.empty());
}

TEST_CASE("readFileBytes refuses missing files and directories with empty bytes")
{
    TempDir dir;
    std::vector<std::uint8_t> bytes{1, 2, 3};

    REQUIRE_FALSE(readFileBytes(dir.path / "missing.bin", bytes));
    REQUIRE(bytes.empty());

    bytes = {1, 2, 3};
    REQUIRE_FALSE(readFileBytes(dir.path, bytes));
    REQUIRE(bytes.empty());
}

TEST_CASE("readFileBytes enforces the size cap inclusively")
{
    TempDir dir;
    writeRaw(dir.path / "four.bin", "abcd");

    std::vector<std::uint8_t> bytes;
    REQUIRE(readFileBytes(dir.path / "four.bin", bytes, 4));
    REQUIRE(bytes.size() == 4);
    REQUIRE_FALSE(readFileBytes(dir.path / "four.bin", bytes, 3));
    REQUIRE(bytes.empty());
}

TEST_CASE("writeFileAtomically creates and replaces a file and leaves no temporary behind")
{
    TempDir dir;
    const fs::path target = dir.path / "result.json";

    REQUIRE(writeFileAtomically(target, {'o', 'l', 'd', '!'}));
    REQUIRE(writeFileAtomically(target, {'n', 'e', 'w'}));

    std::vector<std::uint8_t> bytes;
    REQUIRE(readFileBytes(target, bytes));
    REQUIRE(bytes == std::vector<std::uint8_t>{'n', 'e', 'w'});
    REQUIRE(dir.entries() == 1);
}

TEST_CASE("writeFileAtomically writes an empty file")
{
    TempDir dir;
    const fs::path target = dir.path / "empty.json";

    REQUIRE(writeFileAtomically(target, {}));
    REQUIRE(fs::exists(target));
    REQUIRE(fs::file_size(target) == 0);
}

TEST_CASE("writeFileAtomically fails into a missing directory without creating it")
{
    TempDir dir;
    const fs::path target = dir.path / "missing" / "result.json";

    REQUIRE_FALSE(writeFileAtomically(target, {'x'}));
    REQUIRE_FALSE(fs::exists(dir.path / "missing"));
}

TEST_CASE("writeFileAtomically keeps the old file when the target cannot be replaced")
{
    TempDir dir;
    const fs::path target = dir.path / "taken";
    fs::create_directories(target / "child");

    REQUIRE_FALSE(writeFileAtomically(target, {'x'}));
    REQUIRE(fs::is_directory(target / "child"));
    REQUIRE(dir.entries() == 1);
}

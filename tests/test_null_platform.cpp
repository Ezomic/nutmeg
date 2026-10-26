// SPDX-License-Identifier: Apache-2.0
// Copyright (c) Nutmeg contributors

#include "nutmeg/platform/null_platform.h"

#include <catch2/catch_test_macros.hpp>

using namespace nutmeg;

TEST_CASE("the null platform clock advances one tick per pump and never reads wall time")
{
    NullPlatform platform;
    REQUIRE(platform.init({}));
    REQUIRE(platform.now() == 0.0);
    platform.pumpEvents();
    platform.pumpEvents();
    REQUIRE(platform.now() == 2.0 * NullPlatform::kTick);
    REQUIRE(platform.pumpCount() == 2);
}

TEST_CASE("scripted input comes back exactly as set")
{
    NullPlatform platform;
    const InputFrame frame{InputFrame::Pass | InputFrame::Sprint, 0.5f, -1.0f};
    platform.setInput(frame);
    REQUIRE(platform.input() == frame);
}

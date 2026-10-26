// SPDX-License-Identifier: Apache-2.0
// Copyright (c) Nutmeg contributors

#include "nutmeg/platform/null_platform.h"

namespace nutmeg {

bool NullPlatform::init(const WindowDesc&)
{
    return true;
}

void NullPlatform::pumpEvents()
{
    ++pumps_;
}

double NullPlatform::now() const
{
    return static_cast<double>(pumps_) * kTick;
}

}

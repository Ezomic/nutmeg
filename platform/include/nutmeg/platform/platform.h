// SPDX-License-Identifier: Apache-2.0
// Copyright (c) Nutmeg contributors

#pragma once

#include "nutmeg/core/input.h"

#include <string>

namespace nutmeg {

struct WindowDesc {
    std::string title = "Nutmeg";
    int width = 1280;
    int height = 720;
    bool hidden = false; // created never shown: nothing appears on the desktop
};

struct Extent {
    int width = 0;
    int height = 0;
};

class Platform {
public:
    virtual ~Platform() = default;

    virtual bool init(const WindowDesc& desc) = 0;
    virtual void pumpEvents() = 0;
    virtual bool quitRequested() const = 0;
    virtual void requestQuit() = 0;
    virtual double now() const = 0;
    virtual Extent drawableSize() const = 0;

    // The player's input as of the last pump, in simulation terms.
    virtual InputFrame input() const = 0;
};

}

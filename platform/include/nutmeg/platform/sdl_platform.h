#pragma once

#include "nutmeg/platform/platform.h"

struct SDL_Window;
struct SDL_Gamepad;

namespace nutmeg {

class SdlPlatform final : public Platform {
public:
    SdlPlatform() = default;
    ~SdlPlatform() override;
    SdlPlatform(const SdlPlatform&) = delete;
    SdlPlatform& operator=(const SdlPlatform&) = delete;

    bool init(const WindowDesc& desc) override;
    void pumpEvents() override;
    bool quitRequested() const override { return quit_; }
    void requestQuit() override { quit_ = true; }
    double now() const override;
    Extent drawableSize() const override;
    InputFrame input() const override { return input_; }

    SDL_Window* window() const { return window_; }

    // Keys that went down during the last pump, for toggles that must not repeat while held.
    bool keyPressed(int scancode) const;

private:
    void openFirstGamepad();

    static constexpr int kMaxPressed = 16;

    SDL_Window* window_ = nullptr;
    SDL_Gamepad* gamepad_ = nullptr;
    bool quit_ = false;
    InputFrame input_;
    int pressed_[kMaxPressed] = {};
    int pressedCount_ = 0;
};

}

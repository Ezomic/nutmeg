// Window, event pump and gamepad handling adapted from the Ichor engine's SDL
// platform (Apache-2.0), reduced to the generic parts.
#include "nutmeg/platform/sdl_platform.h"

#include "nutmeg/core/log.h"
#include "nutmeg/platform/input_mapping.h"

#include <SDL3/SDL.h>

namespace nutmeg {

SdlPlatform::~SdlPlatform()
{
    if (gamepad_ != nullptr) {
        SDL_CloseGamepad(gamepad_);
    }
    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
    }
    SDL_Quit();
}

bool SdlPlatform::init(const WindowDesc& desc)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        logf(LogLevel::Error, "SDL_Init failed: {}", SDL_GetError());
        return false;
    }
    window_ = SDL_CreateWindow(desc.title.c_str(), desc.width, desc.height, SDL_WINDOW_RESIZABLE | (desc.hidden ? SDL_WINDOW_HIDDEN : 0));
    if (window_ == nullptr) {
        logf(LogLevel::Error, "SDL_CreateWindow failed: {}", SDL_GetError());
        return false;
    }
    openFirstGamepad();
    return true;
}

void SdlPlatform::openFirstGamepad()
{
    if (gamepad_ != nullptr) {
        return;
    }
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (ids != nullptr && count > 0) {
        gamepad_ = SDL_OpenGamepad(ids[0]);
    }
    SDL_free(ids);
}

void SdlPlatform::pumpEvents()
{
    pressedCount_ = 0;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
            quit_ = true;
            break;
        case SDL_EVENT_KEY_DOWN:
            if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
                quit_ = true;
            }
            if (!event.key.repeat && pressedCount_ < kMaxPressed) {
                pressed_[pressedCount_++] = static_cast<int>(event.key.scancode);
            }
            break;
        case SDL_EVENT_GAMEPAD_ADDED:
            openFirstGamepad();
            break;
        case SDL_EVENT_GAMEPAD_REMOVED:
            if (gamepad_ != nullptr && SDL_GetGamepadID(gamepad_) == event.gdevice.which) {
                SDL_CloseGamepad(gamepad_);
                gamepad_ = nullptr;
                openFirstGamepad();
            }
            break;
        default:
            break;
        }
    }

    const bool* held = SDL_GetKeyboardState(nullptr);
    KeyboardState keys;
    keys.left = held[SDL_SCANCODE_A] || held[SDL_SCANCODE_LEFT];
    keys.right = held[SDL_SCANCODE_D] || held[SDL_SCANCODE_RIGHT];
    keys.up = held[SDL_SCANCODE_W] || held[SDL_SCANCODE_UP];
    keys.down = held[SDL_SCANCODE_S] || held[SDL_SCANCODE_DOWN];
    keys.pass = held[SDL_SCANCODE_J] || held[SDL_SCANCODE_SPACE];
    keys.through = held[SDL_SCANCODE_K];
    keys.shoot = held[SDL_SCANCODE_L];
    keys.sprint = held[SDL_SCANCODE_LSHIFT] || held[SDL_SCANCODE_RSHIFT];
    keys.tackle = held[SDL_SCANCODE_E];
    keys.switchPlayer = held[SDL_SCANCODE_Q];
    keys.pause = held[SDL_SCANCODE_P];

    GamepadState pad;
    if (gamepad_ != nullptr) {
        pad.connected = true;
        pad.stickX = static_cast<float>(SDL_GetGamepadAxis(gamepad_, SDL_GAMEPAD_AXIS_LEFTX)) / 32767.0f;
        pad.stickY = static_cast<float>(SDL_GetGamepadAxis(gamepad_, SDL_GAMEPAD_AXIS_LEFTY)) / 32767.0f;
        pad.dpadLeft = SDL_GetGamepadButton(gamepad_, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
        pad.dpadRight = SDL_GetGamepadButton(gamepad_, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
        pad.dpadUp = SDL_GetGamepadButton(gamepad_, SDL_GAMEPAD_BUTTON_DPAD_UP);
        pad.dpadDown = SDL_GetGamepadButton(gamepad_, SDL_GAMEPAD_BUTTON_DPAD_DOWN);
        pad.south = SDL_GetGamepadButton(gamepad_, SDL_GAMEPAD_BUTTON_SOUTH);
        pad.north = SDL_GetGamepadButton(gamepad_, SDL_GAMEPAD_BUTTON_NORTH);
        pad.east = SDL_GetGamepadButton(gamepad_, SDL_GAMEPAD_BUTTON_EAST);
        pad.west = SDL_GetGamepadButton(gamepad_, SDL_GAMEPAD_BUTTON_WEST);
        pad.leftShoulder = SDL_GetGamepadButton(gamepad_, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
        pad.rightShoulder = SDL_GetGamepadButton(gamepad_, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
        pad.start = SDL_GetGamepadButton(gamepad_, SDL_GAMEPAD_BUTTON_START);
    }
    input_ = mergeInput(mapKeyboard(keys), mapGamepad(pad));
}

bool SdlPlatform::keyPressed(int scancode) const
{
    for (int i = 0; i < pressedCount_; ++i) {
        if (pressed_[i] == scancode) {
            return true;
        }
    }
    return false;
}

double SdlPlatform::now() const
{
    return static_cast<double>(SDL_GetTicksNS()) * 1e-9;
}

Extent SdlPlatform::drawableSize() const
{
    Extent extent;
    SDL_GetWindowSizeInPixels(window_, &extent.width, &extent.height);
    return extent;
}

}

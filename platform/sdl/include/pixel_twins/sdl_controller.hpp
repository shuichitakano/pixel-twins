#pragma once

#include "pixel_twins/controller.hpp"

#include <array>

union SDL_Event;
struct SDL_Gamepad;

namespace pixel_twins::sdl {

class ControllerInput {
public:
    ControllerInput();
    ~ControllerInput();

    ControllerInput(const ControllerInput&) = delete;
    ControllerInput& operator=(const ControllerInput&) = delete;

    void processEvent(const SDL_Event& event) noexcept;
    void update(Controllers& controllers) noexcept;

private:
    void openAvailableGamepads() noexcept;
    void removeDisconnectedGamepads() noexcept;

    std::array<SDL_Gamepad*, kControllerCount> gamepads_{};
};

} // namespace pixel_twins::sdl

#pragma once

#include "pixel_twins/controller.hpp"

#include <array>
#include <cstdint>

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
    [[nodiscard]] std::uint8_t takeBgmVoiceToggleMask() noexcept;

private:
    void openAvailableGamepads() noexcept;
    void removeDisconnectedGamepads() noexcept;

    std::array<SDL_Gamepad*, kControllerCount> gamepads_{};
    std::uint8_t bgmVoiceToggleMask_ = 0;
};

} // namespace pixel_twins::sdl

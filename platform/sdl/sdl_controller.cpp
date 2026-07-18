#include "pixel_twins/sdl_controller.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace pixel_twins::sdl {
namespace {

constexpr std::int16_t kAxisDeadzone = 8000;

[[nodiscard]] std::uint16_t bit(ControllerButton button) noexcept {
    return buttonMask(button);
}

void setButton(std::uint16_t& buttons, ControllerButton button, bool pressed) noexcept {
    if (pressed) buttons = static_cast<std::uint16_t>(buttons | bit(button));
}

[[nodiscard]] std::int16_t axis(SDL_Gamepad* gamepad, SDL_GamepadAxis which) noexcept {
    const auto value = SDL_GetGamepadAxis(gamepad, which);
    return value > -kAxisDeadzone && value < kAxisDeadzone ? 0 : value;
}

void readGamepad(ControllerSample& sample, SDL_Gamepad* gamepad) noexcept {
    sample.connected = true;
    sample.gamepad = true;
    sample.x = axis(gamepad, SDL_GAMEPAD_AXIS_LEFTX);
    sample.y = axis(gamepad, SDL_GAMEPAD_AXIS_LEFTY);
    setButton(sample.buttons, ControllerButton::dpadLeft,
              SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT));
    setButton(sample.buttons, ControllerButton::dpadUp,
              SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP));
    setButton(sample.buttons, ControllerButton::dpadRight,
              SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT));
    setButton(sample.buttons, ControllerButton::dpadDown,
              SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN));
    setButton(sample.buttons, ControllerButton::choiceLeft,
              SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_WEST));
    setButton(sample.buttons, ControllerButton::choiceUp,
              SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_NORTH));
    setButton(sample.buttons, ControllerButton::choiceRight,
              SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_EAST));
    setButton(sample.buttons, ControllerButton::choiceDown,
              SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_SOUTH));
    setButton(sample.buttons, ControllerButton::start,
              SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_START));
    setButton(sample.buttons, ControllerButton::back,
              SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_BACK));

}

void applyDigitalAxes(ControllerSample& sample) noexcept {
    const bool left = (sample.buttons & bit(ControllerButton::dpadLeft)) != 0;
    const bool right = (sample.buttons & bit(ControllerButton::dpadRight)) != 0;
    const bool up = (sample.buttons & bit(ControllerButton::dpadUp)) != 0;
    const bool down = (sample.buttons & bit(ControllerButton::dpadDown)) != 0;
    if (left != right) sample.x = left ? INT16_MIN : INT16_MAX;
    if (up != down) sample.y = up ? INT16_MIN : INT16_MAX;
}

void readKeyboard(std::array<ControllerSample, kControllerCount>& samples) noexcept {
    const bool* keys = SDL_GetKeyboardState(nullptr);
    auto key = [keys](SDL_Scancode code) { return keys[code]; };

    auto& p1 = samples[0];
    p1.connected = true;
    setButton(p1.buttons, ControllerButton::dpadLeft, key(SDL_SCANCODE_A));
    setButton(p1.buttons, ControllerButton::dpadUp, key(SDL_SCANCODE_W));
    setButton(p1.buttons, ControllerButton::dpadRight, key(SDL_SCANCODE_D));
    setButton(p1.buttons, ControllerButton::dpadDown, key(SDL_SCANCODE_S));
    setButton(p1.buttons, ControllerButton::choiceLeft, key(SDL_SCANCODE_1));
    setButton(p1.buttons, ControllerButton::choiceUp, key(SDL_SCANCODE_2));
    setButton(p1.buttons, ControllerButton::choiceRight, key(SDL_SCANCODE_3));
    setButton(p1.buttons, ControllerButton::choiceDown, key(SDL_SCANCODE_4));

    auto& p2 = samples[1];
    p2.connected = true;
    setButton(p2.buttons, ControllerButton::dpadLeft, key(SDL_SCANCODE_LEFT));
    setButton(p2.buttons, ControllerButton::dpadUp, key(SDL_SCANCODE_UP));
    setButton(p2.buttons, ControllerButton::dpadRight, key(SDL_SCANCODE_RIGHT));
    setButton(p2.buttons, ControllerButton::dpadDown, key(SDL_SCANCODE_DOWN));
    setButton(p2.buttons, ControllerButton::choiceLeft, key(SDL_SCANCODE_7));
    setButton(p2.buttons, ControllerButton::choiceUp, key(SDL_SCANCODE_8));
    setButton(p2.buttons, ControllerButton::choiceRight, key(SDL_SCANCODE_9));
    setButton(p2.buttons, ControllerButton::choiceDown, key(SDL_SCANCODE_0));

    if (key(SDL_SCANCODE_P) || key(SDL_SCANCODE_SPACE) || key(SDL_SCANCODE_ESCAPE)) {
        p1.buttons = static_cast<std::uint16_t>(p1.buttons | bit(ControllerButton::start));
    }
    applyDigitalAxes(p1);
    applyDigitalAxes(p2);
}

} // namespace

ControllerInput::ControllerInput() {
    if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
        throw std::runtime_error(std::string("SDLゲームパッドの初期化に失敗しました: ") + SDL_GetError());
    }
    openAvailableGamepads();
}

ControllerInput::~ControllerInput() {
    for (auto* gamepad : gamepads_) SDL_CloseGamepad(gamepad);
    SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
}

void ControllerInput::processEvent(const SDL_Event& event) noexcept {
    if (event.type == SDL_EVENT_GAMEPAD_ADDED) openAvailableGamepads();
    if (event.type == SDL_EVENT_GAMEPAD_REMOVED) removeDisconnectedGamepads();
}

void ControllerInput::update(Controllers& controllers) noexcept {
    removeDisconnectedGamepads();
    std::array<ControllerSample, kControllerCount> samples{};
    for (std::size_t i = 0; i < gamepads_.size(); ++i) {
        if (gamepads_[i] != nullptr) readGamepad(samples[i], gamepads_[i]);
    }
    readKeyboard(samples);
    controllers.update(samples);
}

void ControllerInput::openAvailableGamepads() noexcept {
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    for (int i = 0; i < count; ++i) {
        bool alreadyOpen = false;
        for (auto* gamepad : gamepads_) {
            if (gamepad != nullptr && SDL_GetGamepadID(gamepad) == ids[i]) alreadyOpen = true;
        }
        if (alreadyOpen) continue;
        const auto slot = std::find(gamepads_.begin(), gamepads_.end(), nullptr);
        if (slot == gamepads_.end()) break;
        *slot = SDL_OpenGamepad(ids[i]);
    }
    SDL_free(ids);
}

void ControllerInput::removeDisconnectedGamepads() noexcept {
    for (auto& gamepad : gamepads_) {
        if (gamepad != nullptr && !SDL_GamepadConnected(gamepad)) {
            SDL_CloseGamepad(gamepad);
            gamepad = nullptr;
        }
    }
}

} // namespace pixel_twins::sdl

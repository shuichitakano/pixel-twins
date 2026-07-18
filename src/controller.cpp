#include "pixel_twins/controller.hpp"

namespace pixel_twins {

bool ControllerState::isHeld(ControllerButton button) const noexcept {
    return (held & buttonMask(button)) != 0;
}

bool ControllerState::isPressed(ControllerButton button) const noexcept {
    return (pressed & buttonMask(button)) != 0;
}

bool ControllerState::isReleased(ControllerButton button) const noexcept {
    return (released & buttonMask(button)) != 0;
}

void Controllers::update(const std::array<ControllerSample, kControllerCount>& samples) noexcept {
    for (std::size_t i = 0; i < kControllerCount; ++i) {
        auto& state = states_[i];
        const auto previous = state.held;
        const auto current = samples[i].buttons;
        state.x = samples[i].x;
        state.y = samples[i].y;
        state.held = current;
        state.pressed = static_cast<std::uint16_t>(current & ~previous);
        state.released = static_cast<std::uint16_t>(previous & ~current);
        state.connected = samples[i].connected;
        state.gamepad = samples[i].gamepad;
    }
}

} // namespace pixel_twins

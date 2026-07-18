#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace pixel_twins {

constexpr std::size_t kControllerCount = 2;

enum class ControllerButton : std::uint16_t {
    dpadLeft = 1u << 0,
    dpadUp = 1u << 1,
    dpadRight = 1u << 2,
    dpadDown = 1u << 3,
    choiceLeft = 1u << 4,
    choiceUp = 1u << 5,
    choiceRight = 1u << 6,
    choiceDown = 1u << 7,
    start = 1u << 8,
    back = 1u << 9,
};

[[nodiscard]] constexpr std::uint16_t buttonMask(ControllerButton button) noexcept {
    return static_cast<std::uint16_t>(button);
}

// 対象固有の入力処理が1フレーム分だけ生成する値。
struct ControllerSample {
    std::int16_t x = 0;
    std::int16_t y = 0;
    std::uint16_t buttons = 0;
    bool connected = false;
    bool gamepad = false;
};

struct ControllerState {
    std::int16_t x = 0;
    std::int16_t y = 0;
    std::uint16_t held = 0;
    std::uint16_t pressed = 0;
    std::uint16_t released = 0;
    bool connected = false;
    bool gamepad = false;

    [[nodiscard]] bool isHeld(ControllerButton button) const noexcept;
    [[nodiscard]] bool isPressed(ControllerButton button) const noexcept;
    [[nodiscard]] bool isReleased(ControllerButton button) const noexcept;
};

static_assert(sizeof(ControllerSample) == 8);
static_assert(sizeof(ControllerState) == 12);

class Controllers {
public:
    void update(const std::array<ControllerSample, kControllerCount>& samples) noexcept;

    [[nodiscard]] const ControllerState& operator[](std::size_t player) const noexcept {
        return states_[player];
    }

private:
    std::array<ControllerState, kControllerCount> states_{};
};

} // namespace pixel_twins

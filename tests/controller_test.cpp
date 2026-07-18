#include "pixel_twins/controller.hpp"

#include "test_check.hpp"

#include <array>

int main() {
    using namespace pixel_twins;
    Controllers controllers;
    std::array<ControllerSample, kControllerCount> samples{};

    samples[0].x = -1234;
    samples[0].buttons = buttonMask(ControllerButton::choiceLeft);
    samples[0].connected = true;
    controllers.update(samples);
    check(controllers[0].x == -1234);
    check(controllers[0].connected);
    check(controllers[0].isHeld(ControllerButton::choiceLeft));
    check(controllers[0].isPressed(ControllerButton::choiceLeft));
    check(!controllers[0].isReleased(ControllerButton::choiceLeft));

    controllers.update(samples);
    check(controllers[0].isHeld(ControllerButton::choiceLeft));
    check(!controllers[0].isPressed(ControllerButton::choiceLeft));

    samples[0].buttons = buttonMask(ControllerButton::choiceDown);
    controllers.update(samples);
    check(controllers[0].isReleased(ControllerButton::choiceLeft));
    check(controllers[0].isPressed(ControllerButton::choiceDown));

    return 0;
}

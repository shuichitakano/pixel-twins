#pragma once

#include "pixel_twins/framebuffer.hpp"

#include <array>
#include <cstdint>

struct SDL_Renderer;
struct SDL_Texture;
struct SDL_Window;

namespace pixel_twins::sdl {

class Presenter {
public:
    explicit Presenter(int scale = 4);
    ~Presenter();

    Presenter(const Presenter&) = delete;
    Presenter& operator=(const Presenter&) = delete;

    [[nodiscard]] bool processEvents() const noexcept;
    void present(const Framebuffer& framebuffer);

private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    std::array<std::uint8_t, kFramebufferSize * 4> rgba_{};
};

} // namespace pixel_twins::sdl

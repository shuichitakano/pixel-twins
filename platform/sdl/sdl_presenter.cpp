#include "pixel_twins/sdl_presenter.hpp"
#include "pixel_twins/sdl_controller.hpp"

#include <SDL3/SDL.h>

#include <cstddef>
#include <stdexcept>
#include <string>

namespace pixel_twins::sdl {
namespace {

[[nodiscard]] std::runtime_error sdlError(const char* operation) {
    return std::runtime_error(std::string(operation) + ": " + SDL_GetError());
}

} // namespace

Presenter::Presenter(int scale, bool vsync) {
    if (scale <= 0) {
        throw std::invalid_argument("表示倍率は1以上でなければなりません");
    }
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        throw sdlError("SDLの初期化に失敗しました");
    }
    if (!SDL_CreateWindowAndRenderer("Pixel Twins",
                                     static_cast<int>(kScreenWidth) * scale,
                                     static_cast<int>(kScreenHeight) * scale,
                                     0,
                                     &window_,
                                     &renderer_)) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        throw sdlError("SDLウィンドウの作成に失敗しました");
    }
    if (vsync && !SDL_SetRenderVSync(renderer_, 1)) {
        SDL_DestroyRenderer(renderer_);
        SDL_DestroyWindow(window_);
        renderer_ = nullptr;
        window_ = nullptr;
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        throw sdlError("SDL VSyncの有効化に失敗しました");
    }

    texture_ = SDL_CreateTexture(renderer_,
                                 SDL_PIXELFORMAT_RGBA32,
                                 SDL_TEXTUREACCESS_STREAMING,
                                 static_cast<int>(kScreenWidth),
                                 static_cast<int>(kScreenHeight));
    if (texture_ == nullptr) {
        SDL_DestroyRenderer(renderer_);
        SDL_DestroyWindow(window_);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        throw sdlError("SDLテクスチャの作成に失敗しました");
    }
    SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_NEAREST);
}

Presenter::~Presenter() {
    SDL_DestroyTexture(texture_);
    SDL_DestroyRenderer(renderer_);
    SDL_DestroyWindow(window_);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

bool Presenter::processEvents(ControllerInput* controllerInput) const noexcept {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            return false;
        }
        if (controllerInput != nullptr) controllerInput->processEvent(event);
    }
    return true;
}

void Presenter::present(const Framebuffer& framebuffer) {
    const auto& source = framebuffer.displayBuffer();
    const auto& palette = framebuffer.palette();

    for (std::size_t i = 0; i < source.size(); ++i) {
        const auto color = palette[source[i]];
        const auto destination = i * 4;
        rgba_[destination] = color.r;
        rgba_[destination + 1] = color.g;
        rgba_[destination + 2] = color.b;
        rgba_[destination + 3] = 255;
    }

    if (!SDL_UpdateTexture(texture_, nullptr, rgba_.data(), static_cast<int>(kScreenWidth * 4))) {
        throw sdlError("SDLテクスチャの更新に失敗しました");
    }
    if (!SDL_RenderClear(renderer_) || !SDL_RenderTexture(renderer_, texture_, nullptr, nullptr)
        || !SDL_RenderPresent(renderer_)) {
        throw sdlError("SDL画面の表示に失敗しました");
    }
}

} // namespace pixel_twins::sdl

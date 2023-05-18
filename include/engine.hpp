#pragma once

#include "graphics_device.hpp"

struct SDL_Window;

namespace cpt
{
class Engine
{
  public:
    Engine(const std::string_view windowTitle, const uint32_t windowWidth, const uint32_t windowHeight);
    ~Engine();

    void run();

  private:
    void update(const float deltaTime);
    void render();

  private:
    uint32_t m_windowWidth{};
    uint32_t m_windowHeight{};
    SDL_Window *m_window{};
    HWND m_windowHandle{};

    std::unique_ptr<GraphicsDevice> m_graphicsDevice{};

    uint32_t m_frameNumber{};
};
} // namespace cpt
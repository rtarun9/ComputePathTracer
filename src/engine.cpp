#include "engine.hpp"
#include "utils.hpp"

#include <SDL.h>
#include <SDL_syswm.h>

namespace cpt
{
Engine::Engine(const std::string_view windowTitle, const uint32_t windowWidth, const uint32_t windowHeight)
{
    // Initialize SDL2.
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        utils::fatalError("Failed to initialize SDL2");
    }

    // Create SDL2 window.
    m_window = SDL_CreateWindow(windowTitle.data(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth,
                                windowHeight, SDL_WINDOW_ALLOW_HIGHDPI);
    if (!m_window)
    {
        utils::fatalError("Failed to create SDL2 window");
    }

    // Get window handle.
    SDL_SysWMinfo wmInfo{};
    SDL_VERSION(&wmInfo.version);

    SDL_GetWindowWMInfo(m_window, &wmInfo);
    m_windowHandle = wmInfo.info.win.window;

    // Create the graphics device, which in turn will setup the graphics backend.
    m_graphicsDevice = std::make_unique<GraphicsDevice>(m_windowWidth, m_windowHeight, m_windowHandle);
}   

Engine::~Engine()
{
    SDL_DestroyWindow(m_window);
    SDL_Quit();
}

void Engine::run()
{
    try
    {
        bool quit = false;
        while (!quit)
        {
            SDL_Event event{};
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_QUIT)
                {
                    quit = true;
                }

                const uint8_t *keyboardState = SDL_GetKeyboardState(nullptr);
                if (keyboardState[SDL_SCANCODE_ESCAPE])
                {
                    quit = true;
                }
            }
        }
    }
    catch (const std::exception &exception)
    {
        std::cerr << "[Exception Caught] :: " << exception.what() << ".\n";
        return;
    }
}

} // namespace cpt
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

    // Get the raw window handle.
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
    std::chrono::high_resolution_clock clock{};
    std::chrono::high_resolution_clock::time_point previousFrameTimePoint{};

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
            
            const auto currentFrameTimePoint = clock.now();
            const float deltaTime =
                std::chrono::duration_cast<std::chrono::milliseconds>(currentFrameTimePoint - previousFrameTimePoint)
                    .count();
            previousFrameTimePoint = currentFrameTimePoint;

            update(deltaTime);
            render();

            m_frameNumber++;
        }
    }
    catch (const std::exception &exception)
    {
        std::cerr << "[Exception Caught] :: " << exception.what() << ".\n";
        return;
    }
}

void Engine::update(const float deltaTime)
{
}

void Engine::render()
{
    // Reset the command list and associated command allocator for this frame.
    const uint32_t currentFrameIndex = m_graphicsDevice->m_currentFrameIndex;
    auto &commandAllocator = m_graphicsDevice->m_commandAllocators[currentFrameIndex];
    auto &commandList = m_graphicsDevice->m_commandList;

    commandAllocator->Reset();
    utils::dxCheck(commandList->Reset(commandAllocator.Get(), nullptr));

    // Transition backbuffer from presentation to render target state.
    const D3D12_RESOURCE_BARRIER presentationToRtBarrier = {
        .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
        .Transition =
            {
                .pResource = m_graphicsDevice->m_rtvBackBufferResources[currentFrameIndex].Get(),
                .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                .StateBefore = D3D12_RESOURCE_STATE_PRESENT,
                .StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET,
            },
    };

    commandList->ResourceBarrier(1u, &presentationToRtBarrier);

    // Clear render target view.
    const std::array<float, 4> clearColor = {0.2f, 0.2f, std::abs((float)sin(m_frameNumber / 180.0f)), 1.0f};
    D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle =
        m_graphicsDevice->m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    rtvDescriptorHandle.ptr += m_graphicsDevice->m_rtvDescriptorHandleIncrementSize * currentFrameIndex;

    commandList->ClearRenderTargetView(rtvDescriptorHandle, clearColor.data(), 0u, nullptr);

    // Transition backbuffer from render target to presentation state.
    const D3D12_RESOURCE_BARRIER rtToPresentationBarrier = {
        .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
        .Transition =
            {
                .pResource = m_graphicsDevice->m_rtvBackBufferResources[currentFrameIndex].Get(),
                .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                .StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET,
                .StateAfter = D3D12_RESOURCE_STATE_PRESENT,
            },
    };

    commandList->ResourceBarrier(1u, &rtToPresentationBarrier);

    // Execute command list.
    utils::dxCheck(commandList->Close());
    std::array<ID3D12CommandList *, 1u> commandLists = {
        commandList.Get(),
    };

    m_graphicsDevice->m_directCommandQueue->ExecuteCommandLists(1u, commandLists.data());
    
    // Present to swapchain.
    utils::dxCheck(m_graphicsDevice->m_swapchain->Present(1u, 0u));

    m_graphicsDevice->m_frameFenceValues[currentFrameIndex] = m_graphicsDevice->signal();

    // Wait for next frame's resources to be out of reference.
    m_graphicsDevice->m_currentFrameIndex = m_graphicsDevice->m_swapchain->GetCurrentBackBufferIndex();
    m_graphicsDevice->waitForFenceValue(m_graphicsDevice->m_frameFenceValues[m_graphicsDevice->m_currentFrameIndex]);
}
} // namespace cpt
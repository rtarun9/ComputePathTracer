#pragma once

#include "graphics_device.hpp"

struct SDL_Window;

namespace cpt
{
struct alignas(256) GlobalConstantBuffer
{
    DirectX::XMFLOAT2 screenDimensions{};
};

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

    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature{};
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState{};

    Microsoft::WRL::ComPtr<ID3D12Resource> m_computeShaderOutputTexture{};
    uint32_t m_computeShaderUAVHeapIndex{0u};

    Microsoft::WRL::ComPtr<ID3D12Resource> m_globalCBufferResource{};
    GlobalConstantBuffer m_globalCBufferData{};
    uint8_t *m_globalCBufferPtr{};
    uint32_t m_globalCBufferHeapIndex{1u};

    uint32_t m_frameNumber{};
};
} // namespace cpt
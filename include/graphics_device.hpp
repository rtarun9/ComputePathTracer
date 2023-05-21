#pragma once

namespace cpt
{
class GraphicsDevice
{
  public:
    GraphicsDevice(const uint32_t &windowWidth, const uint32_t &windowHeight, const HWND &windowHandle);
    ~GraphicsDevice() = default;

    // Command queue related operations.
    uint64_t signal();
    void waitForFenceValue(const uint64_t fenceValue);
    void flushDirectCommandQueue();

  private:
    void initGraphicsBackend();

  public:
    constexpr static inline uint32_t FRAMES_IN_FLIGHT = 3u;

    const uint32_t &m_windowWidth;
    const uint32_t &m_windowHeight;
    const HWND &m_windowHandle;

    Microsoft::WRL::ComPtr<ID3D12Debug3> m_debug{};
    Microsoft::WRL::ComPtr<IDXGIFactory6> m_factory{};
    Microsoft::WRL::ComPtr<IDXGIAdapter2> m_adapter{};

    Microsoft::WRL::ComPtr<ID3D12Device5> m_device{};
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_directCommandQueue{};
    std::array<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>, FRAMES_IN_FLIGHT> m_commandAllocators{};
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList3> m_commandList{};

    Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swapchain{};

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_cbvSrvUavDescriptorHeap{};
    uint32_t m_cbvSrvUavDescriptorHandleIncrementSize{};

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvDescriptorHeap{};
    uint32_t m_rtvDescriptorHandleIncrementSize{};

    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, FRAMES_IN_FLIGHT> m_rtvBackBufferResources{};
    std::array<D3D12_CPU_DESCRIPTOR_HANDLE, FRAMES_IN_FLIGHT> m_rtvBackBufferCPUDescriptorHandle{};

    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence{};
    uint64_t m_monotonicallyIncreasingFenceValue{};
    std::array<uint64_t, FRAMES_IN_FLIGHT> m_frameFenceValues{};

    uint32_t m_currentFrameIndex{};
};
} // namespace cpt
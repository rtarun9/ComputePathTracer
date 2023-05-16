#include "graphics_device.hpp"

#include "utils.hpp"

namespace cpt
{
GraphicsDevice::GraphicsDevice(const uint32_t windowWidth, const uint32_t windowHeight, const HWND windowHandle)
    : m_windowWidth(windowWidth), m_windowHeight(windowHeight), m_windowHandle(windowHandle)
{
    initGraphicsBackend();
}

void GraphicsDevice::initGraphicsBackend()
{
    uint32_t factoryCreationFlags = 0;

// Enable the debug layer.
#ifdef _DEBUG
    utils::dxCheck(::D3D12GetDebugInterface(IID_PPV_ARGS(&m_debug)));
    m_debug->EnableDebugLayer();
    m_debug->SetEnableGPUBasedValidation(TRUE);
    m_debug->SetEnableSynchronizedCommandQueueValidation(TRUE);

    factoryCreationFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

    // Create the DXGI factory and get the adapter with highest performance capabilities.
    utils::dxCheck(::CreateDXGIFactory2(factoryCreationFlags, IID_PPV_ARGS(&m_factory)));

    utils::dxCheck(
        m_factory->EnumAdapterByGpuPreference(0u, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&m_adapter)));

    DXGI_ADAPTER_DESC1 adapterDesc{};
    utils::dxCheck(m_adapter->GetDesc1(&adapterDesc));

    std::wcout << "Adapter Chosen :: " << adapterDesc.Description << L".\n";

    // Create the D3D12 device.
    utils::dxCheck(::D3D12CreateDevice(m_adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device)));
    utils::dxCheck(m_device->SetName(L"D3D12 Device"));

    // Enable break points in debug mode.
#ifdef _DEBUG
    Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue{};
    utils::dxCheck(m_device.As(&infoQueue));
    utils::dxCheck(infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE));
    utils::dxCheck(infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE));
    utils::dxCheck(infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE));

#endif

    // Create the direct command queue.
    const D3D12_COMMAND_QUEUE_DESC directCommandQueueDesc = {
        .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
        .Priority = 0u,
        .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
        .NodeMask = 0u,
    };

    utils::dxCheck(m_device->CreateCommandQueue(&directCommandQueueDesc, IID_PPV_ARGS(&m_directCommandQueue)));
    utils::dxCheck(m_directCommandQueue->SetName(L"Direct Command Queue"));

    // Create command allocators.
    for (auto &allocator : m_commandAllocators)
    {
        utils::dxCheck(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)));
        utils::dxCheck(allocator->SetName(L"Command Allocator"));
    }

    // Create command list.
    utils::dxCheck(m_device->CreateCommandList1(0u, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE,
                                                IID_PPV_ARGS(&m_commandList)));

    // Create the swapchain.
    const DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {
        .Width = m_windowWidth,
        .Height = m_windowHeight,
        .Format = DXGI_FORMAT_R10G10B10A2_UNORM,
        .Stereo = FALSE,
        .SampleDesc = {1u, 0u},
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = FRAMES_IN_FLIGHT,
        .Scaling = DXGI_SCALING_NONE,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
        .AlphaMode = DXGI_ALPHA_MODE_IGNORE,
        .Flags = 0u,
    };

    utils::dxCheck(m_factory->CreateSwapChainForHwnd(m_directCommandQueue.Get(), m_windowHandle, &swapchainDesc,
                                                     nullptr, nullptr, &m_swapchain));

    // Create descriptor heaps.
    const D3D12_DESCRIPTOR_HEAP_DESC cbvSrvUavDescriptorHeapDesc = {
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        .NumDescriptors = 3u,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        .NodeMask = 0u,
    };

    utils::dxCheck(
        m_device->CreateDescriptorHeap(&cbvSrvUavDescriptorHeapDesc, IID_PPV_ARGS(&m_cbvSrvUavDescriptorHeap)));
    m_cbvSrvUavDescriptorHandleIncrementSize =
        m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    const D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc = {
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        .NumDescriptors = 3u,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        .NodeMask = 0u,
    };

    utils::dxCheck(m_device->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(&m_rtvDescriptorHeap)));
    m_rtvDescriptorHandleIncrementSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // Create the back buffer RTV view's.
    auto rtvBackBufferDescriptorHandle = m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
    {
        utils::dxCheck(m_swapchain->GetBuffer(i, IID_PPV_ARGS(&m_rtvBackBufferResources[i])));
        m_device->CreateRenderTargetView(m_rtvBackBufferResources[i].Get(), nullptr, rtvBackBufferDescriptorHandle);

        rtvBackBufferDescriptorHandle.ptr += m_rtvDescriptorHandleIncrementSize;
    }
}
} // namespace cpt
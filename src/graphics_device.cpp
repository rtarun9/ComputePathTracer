#include "graphics_device.hpp"

#include "utils.hpp"

namespace cpt
{
GraphicsDevice::GraphicsDevice(const uint32_t &windowWidth, const uint32_t &windowHeight, const HWND &windowHandle)
    : m_windowWidth(windowWidth), m_windowHeight(windowHeight), m_windowHandle(windowHandle)
{
    initGraphicsBackend();
}

// Descriptor heap operations.
D3D12_CPU_DESCRIPTOR_HANDLE GraphicsDevice::getCPUDescriptorHandleAtIndex(const D3D12_DESCRIPTOR_HEAP_TYPE heapType,
                                                                          const uint32_t index)
{
    if (heapType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE cbvSrvUavHandle = m_cbvSrvUavDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        cbvSrvUavHandle.ptr += m_cbvSrvUavDescriptorHandleIncrementSize * index;

        return cbvSrvUavHandle;
    }
    else if (heapType == D3D12_DESCRIPTOR_HEAP_TYPE_RTV)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        rtvHandle.ptr += m_rtvDescriptorHandleIncrementSize * index;

        return rtvHandle;
    }

    return {};
}

D3D12_GPU_DESCRIPTOR_HANDLE GraphicsDevice::getGPUDescriptorHandleAtIndex(const D3D12_DESCRIPTOR_HEAP_TYPE heapType,
                                                                          const uint32_t index)
{
    if (heapType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
    {
        D3D12_GPU_DESCRIPTOR_HANDLE cbvSrvUavHandle = m_cbvSrvUavDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
        cbvSrvUavHandle.ptr += m_cbvSrvUavDescriptorHandleIncrementSize * index;

        return cbvSrvUavHandle;
    }
    else if (heapType == D3D12_DESCRIPTOR_HEAP_TYPE_RTV)
    {
        D3D12_GPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
        rtvHandle.ptr += m_rtvDescriptorHandleIncrementSize * index;

        return rtvHandle;
    }

    return {};
}

// Command queue related operations.
uint64_t GraphicsDevice::signal()
{
    m_monotonicallyIncreasingFenceValue++;
    utils::dxCheck(m_directCommandQueue->Signal(m_fence.Get(), m_monotonicallyIncreasingFenceValue));

    return m_monotonicallyIncreasingFenceValue;
}
void GraphicsDevice::waitForFenceValue(const uint64_t fenceValue)
{
    if (m_fence->GetCompletedValue() < fenceValue)
    {
        utils::dxCheck(m_fence->SetEventOnCompletion(fenceValue, nullptr));
    }
}

void GraphicsDevice::flushDirectCommandQueue()
{
    const uint64_t fenceValue = signal();
    waitForFenceValue(fenceValue);
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
    // DXGI Factory : Generating and querying other DXGI objects.
    utils::dxCheck(::CreateDXGIFactory2(factoryCreationFlags, IID_PPV_ARGS(&m_factory)));

    utils::dxCheck(
        m_factory->EnumAdapterByGpuPreference(0u, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&m_adapter)));

    DXGI_ADAPTER_DESC1 adapterDesc{};
    utils::dxCheck(m_adapter->GetDesc1(&adapterDesc));

    std::wcout << "Adapter Chosen :: " << adapterDesc.Description << L".\n";

    // Create the D3D12 device.
    // The DX12 device is responsable for creation of resources, command lists, command queues, etc.
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
    // Command queue : The execution port of GPU commoands.
    // Command lists record GPU commands, command allocators act as the backing store,
    // and then the command queues actually execute the commands, and perform synchronization as well.
    // There are 3 types of command list : Copy, Compute, and Direct (which is basically Graphics + Copy + Compute).
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
    // Used to record GPU commands. Command allocator is the backing store for commands recorded by command lists.
    utils::dxCheck(m_device->CreateCommandList1(0u, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE,
                                                IID_PPV_ARGS(&m_commandList)));
    utils::dxCheck(m_commandList->SetName(L"Direct Command List"));

    // Create command allocators (we need atleast one per frame).
    for (auto &allocator : m_commandAllocators)
    {
        utils::dxCheck(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)));
        utils::dxCheck(allocator->SetName(L"Direct Command Allocator"));
    }

    // Create the swapchain.
    // Swapchain holds several buffers. We render into the back buffer, and present the front buffer.
    // Once rendering to back buffer is complete, we 'swap' the buffers the front and back buffers are pointing to and
    // continue operations.
    const DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {
        .Width = m_windowWidth,
        .Height = m_windowHeight,
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .Stereo = FALSE,
        .SampleDesc = {1u, 0u},
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = FRAMES_IN_FLIGHT,
        .Scaling = DXGI_SCALING_NONE,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
        .AlphaMode = DXGI_ALPHA_MODE_IGNORE,
        .Flags = 0,
    };

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapchain{};
    utils::dxCheck(m_factory->CreateSwapChainForHwnd(m_directCommandQueue.Get(), m_windowHandle, &swapchainDesc,
                                                     nullptr, nullptr, &swapchain));

    utils::dxCheck(m_factory->MakeWindowAssociation(m_windowHandle, DXGI_MWA_NO_ALT_ENTER));
    utils::dxCheck(swapchain.As(&m_swapchain));

    m_currentFrameIndex = m_swapchain->GetCurrentBackBufferIndex();

    // Create descriptor heaps.
    // Descriptor heap : Contiguous allocation of descriptors.
    // A descriptor describes a resource (such as the format, mipmaps, etc).
    const D3D12_DESCRIPTOR_HEAP_DESC cbvSrvUavDescriptorHeapDesc = {
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        .NumDescriptors = 15u,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        .NodeMask = 0u,
    };

    utils::dxCheck(
        m_device->CreateDescriptorHeap(&cbvSrvUavDescriptorHeapDesc, IID_PPV_ARGS(&m_cbvSrvUavDescriptorHeap)));
    m_cbvSrvUavDescriptorHandleIncrementSize =
        m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    utils::dxCheck(m_cbvSrvUavDescriptorHeap->SetName(L"CBV SRV UAV Descriptor Heap"));

    const D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc = {
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        .NumDescriptors = 4u,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        .NodeMask = 0u,
    };

    utils::dxCheck(m_device->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(&m_rtvDescriptorHeap)));
    utils::dxCheck(m_rtvDescriptorHeap->SetName(L"RTV Descriptor Heap"));
    m_rtvDescriptorHandleIncrementSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // Create the back buffer RTV view's.
    auto rtvBackBufferDescriptorHandle = m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
    {
        utils::dxCheck(m_swapchain->GetBuffer(i, IID_PPV_ARGS(&m_rtvBackBufferResources[i])));
        m_device->CreateRenderTargetView(m_rtvBackBufferResources[i].Get(), nullptr, rtvBackBufferDescriptorHandle);

        rtvBackBufferDescriptorHandle.ptr += m_rtvDescriptorHandleIncrementSize;
    }

    // Create the Fence.
    // When we issue a CommandQueue->Signal call, the fence is signalled only when the GPU reaches that
    // point of execution (i.e if we do commandQueue->Signal after a execute command list, once all instructions
    // recorded in the command list are executed, the fence is signaled. On the CPU side, we can wait for the fence
    // value to be set to the signaled fence value, which means the GPU has completed execution of all instructions
    // prior to the Signal call.
    utils::dxCheck(m_device->CreateFence(0u, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
    utils::dxCheck(m_fence->SetName(L"Fence"));
}
} // namespace cpt
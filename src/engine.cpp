#include "engine.hpp"
#include "utils.hpp"

#include <SDL.h>
#include <SDL_syswm.h>

#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_sdl2.h>

namespace cpt
{
Engine::Engine(const std::string_view windowTitle, const uint32_t windowWidth, const uint32_t windowHeight)
    : m_windowWidth(windowWidth), m_windowHeight(windowHeight)
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

    // Get the project shader directory.
    auto currentDirectory = std::filesystem::current_path();

    while (!std::filesystem::exists(currentDirectory / "shaders"))
    {
        if (currentDirectory.has_parent_path())
        {
            currentDirectory = currentDirectory.parent_path();
        }
        else
        {
            utils::fatalError("Shaders directory not found!");
        }
    }

    const auto shaderDirectory = currentDirectory / "shaders";

    const std::wstring shaderPath = shaderDirectory.wstring() + L"\\path_tracer.hlsl";

    // Compile the path tracer shader.
    Microsoft::WRL::ComPtr<ID3DBlob> shaderBlob{};
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob{};

    uint32_t shaderCompilationOptions = 0;
#ifdef _DEBUG
    shaderCompilationOptions = D3DCOMPILE_DEBUG;
#endif

    const HRESULT hr = ::D3DCompileFromFile(shaderPath.data(), nullptr, nullptr, "CsMain", "cs_5_0",
                                            shaderCompilationOptions, 0u, &shaderBlob, &errorBlob);
    if (errorBlob && FAILED(hr))
    {
        const char *errorMessage = (const char *)errorBlob->GetBufferPointer();
        utils::fatalError(std::format("Shader compilation error :: {}.\n", errorMessage));
    }

    // Create the root signature.
    // A root signature is fairly similar to a function signature, but for the shaders.
    // The path tracing shader only has three shader inputs : A output RWTexture2D<>, and a global constant buffer, and
    // the imgui - offscreen render target. The UAV will be in a descriptor table, since you cant have RWTexture2D<> as
    // a inline descriptor. Same for the offscreen render target srv.
    //
    const std::array<D3D12_DESCRIPTOR_RANGE1, 2u> descriptorRanges = {
        D3D12_DESCRIPTOR_RANGE1{
            .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
            .NumDescriptors = 1u,
            .BaseShaderRegister = 0u,
            .RegisterSpace = 0u,
            .Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE,
            .OffsetInDescriptorsFromTableStart = 0u,
        },
        D3D12_DESCRIPTOR_RANGE1{.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                                .NumDescriptors = 1u,
                                .BaseShaderRegister = 0u,
                                .RegisterSpace = 0u,
                                .Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE,
                                .OffsetInDescriptorsFromTableStart = 0u},
    };

    const std::array<D3D12_ROOT_PARAMETER1, 3u> rootParamters = {
        D3D12_ROOT_PARAMETER1{
            // For the compute shader output texture.
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
            .DescriptorTable =
                {
                    .NumDescriptorRanges = 1u,
                    .pDescriptorRanges = &descriptorRanges[0],
                },
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
        },
        D3D12_ROOT_PARAMETER1{
            // For the offscreen render target output texture.
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
            .DescriptorTable =
                {
                    .NumDescriptorRanges = 1u,
                    .pDescriptorRanges = &descriptorRanges[1],
                },
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
        },
        D3D12_ROOT_PARAMETER1{
            // For the global constant buffer.
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
            .Descriptor =
                {
                    .ShaderRegister = 0u,
                    .RegisterSpace = 0u,
                    .Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE,
                },
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
        },
    };

    const D3D12_STATIC_SAMPLER_DESC linearClampSamplerDesc = {
        .Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR,
        .AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        .AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        .AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        .ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL,
        .ShaderRegister = 0u,
        .RegisterSpace = 0u,
        .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
    };

    const D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc = {
        .Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
        .Desc_1_1 =
            {
                .NumParameters = static_cast<uint32_t>(rootParamters.size()),
                .pParameters = rootParamters.data(),
                .NumStaticSamplers = 1u,
                .pStaticSamplers = &linearClampSamplerDesc,
                .Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE,
            },
    };

    Microsoft::WRL::ComPtr<ID3DBlob> rootSignatureBlob{};
    const HRESULT serializeRootSignatureHresult(
        ::D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &rootSignatureBlob, &errorBlob));
    if (FAILED(serializeRootSignatureHresult))
    {
        const char *message = (const char *)errorBlob->GetBufferPointer();
        utils::fatalError(message);
    }

    utils::dxCheck(m_graphicsDevice->m_device->CreateRootSignature(
        0u, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
    utils::dxCheck(m_rootSignature->SetName(L"Path Tracer Root Signature"));

    // Create the pipeline state.
    // Pipeline state represents the state of current shaders and the FFP objects.
    const D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineStateDesc = {
        .pRootSignature = m_rootSignature.Get(),
        .CS =
            {
                .pShaderBytecode = shaderBlob->GetBufferPointer(),
                .BytecodeLength = shaderBlob->GetBufferSize(),
            },

    };

    utils::dxCheck(
        m_graphicsDevice->m_device->CreateComputePipelineState(&pipelineStateDesc, IID_PPV_ARGS(&m_pipelineState)));
    utils::dxCheck(m_pipelineState->SetName(L"Path Tracer Compute Pipeline State"));

    // Create the resource which will be used as render texture output in compute shader.
    const D3D12_RESOURCE_DESC computeShaderOutputTextureDesc = {
        .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        .Alignment = 0u,
        .Width = m_windowWidth,
        .Height = m_windowHeight,
        .DepthOrArraySize = 1u,
        .MipLevels = 1u,
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .SampleDesc = {1u, 0u},
        .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        .Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
    };

    // Setup heap properties (i.e properties of the contiguous GPU allocation that is created along with the resource).
    const D3D12_HEAP_PROPERTIES heapProperties = {
        .Type = D3D12_HEAP_TYPE_DEFAULT,
        .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
        .CreationNodeMask = 0u,
        .VisibleNodeMask = 0u,
    };

    utils::dxCheck(m_graphicsDevice->m_device->CreateCommittedResource(
        &heapProperties, D3D12_HEAP_FLAG_NONE, &computeShaderOutputTextureDesc, D3D12_RESOURCE_STATE_COPY_SOURCE,
        nullptr, IID_PPV_ARGS(&m_computeShaderOutputTexture)));

    // Create UAV for this resource.
    // A UAV allows for unordered read writes from multiple threads at the same time with no memory conflicts.
    const D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D,
        .Texture2D =
            {
                .MipSlice = 0u,
                .PlaneSlice = 0u,
            },
    };

    const D3D12_CPU_DESCRIPTOR_HANDLE uavCPUDescriptorHandle = m_graphicsDevice->getCPUDescriptorHandleAtIndex(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_computeShaderUAVHeapIndex);

    m_graphicsDevice->m_device->CreateUnorderedAccessView(m_computeShaderOutputTexture.Get(), nullptr, &uavDesc,
                                                          uavCPUDescriptorHandle);

    // Create the constant buffer.
    // As the data is constant for the entire draw call / dispatch call, it is placed in a different memory region
    // called 'Constant Memory'.
    const D3D12_HEAP_PROPERTIES uploadHeapProperties = {
        .Type = D3D12_HEAP_TYPE_UPLOAD,
        .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
        .CreationNodeMask = 1u,
        .VisibleNodeMask = 1u,
    };

    const D3D12_RESOURCE_DESC globalCBufferResourceDesc = {
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Alignment = 0u,
        .Width = sizeof(GlobalConstantBuffer),
        .Height = 1u,
        .DepthOrArraySize = 1u,
        .MipLevels = 1u,
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc = {1u, 0u},
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .Flags = D3D12_RESOURCE_FLAG_NONE,
    };

    utils::dxCheck(m_graphicsDevice->m_device->CreateCommittedResource(
        &uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &globalCBufferResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(&m_globalCBufferResource)));

    // Get the pointer to resource so we can copy data from CPU to the Constant Buffer.
    const D3D12_RANGE readRange{
        .Begin = 0u,
        .End = 0u,
    };

    utils::dxCheck(m_globalCBufferResource->Map(0, &readRange, reinterpret_cast<void **>(&m_globalCBufferPtr)));

    // Create the constant buffer view.
    const D3D12_CPU_DESCRIPTOR_HANDLE cbufferCPUDescriptorHandle = m_graphicsDevice->getCPUDescriptorHandleAtIndex(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_globalCBufferHeapIndex);

    const D3D12_CONSTANT_BUFFER_VIEW_DESC globalCBufferViewDesc = {
        .BufferLocation = m_globalCBufferResource->GetGPUVirtualAddress(),
        .SizeInBytes = sizeof(GlobalConstantBuffer),
    };

    m_graphicsDevice->m_device->CreateConstantBufferView(&globalCBufferViewDesc, cbufferCPUDescriptorHandle);

    // Create the offscreen render target. Must be in the D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE initially, and we
    // should be able to use it as a render target.
    const D3D12_RESOURCE_DESC offscreenRTDesc = {
        .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        .Alignment = 0u,
        .Width = m_windowWidth,
        .Height = m_windowHeight,
        .DepthOrArraySize = 1u,
        .MipLevels = 1u,
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .SampleDesc = {1u, 0u},
        .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        .Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
    };

    const D3D12_CLEAR_VALUE optimizedClearValue = {
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .Color = {0.0f, 0.0f, 0.0f, 1.0f},
    };

    utils::dxCheck(m_graphicsDevice->m_device->CreateCommittedResource(
        &heapProperties, D3D12_HEAP_FLAG_NONE, &offscreenRTDesc, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
        &optimizedClearValue, IID_PPV_ARGS(&m_offscreenRT)));

    // Create the render target view.
    const D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
        .Texture2D = {.MipSlice = 0u, .PlaneSlice = 0u},
    };

    const D3D12_CPU_DESCRIPTOR_HANDLE rtvCPUDescriptorHandle =
        m_graphicsDevice->getCPUDescriptorHandleAtIndex(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, m_offscreenRTHeapIndexRTV);

    m_graphicsDevice->m_device->CreateRenderTargetView(m_offscreenRT.Get(), &rtvDesc, rtvCPUDescriptorHandle);

    // Create the shader resource view.
    const D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
        .Texture2D =
            {
                .MostDetailedMip = 0u,
                .MipLevels = 1u,
                .PlaneSlice = 0u,
            },
    };

    const D3D12_CPU_DESCRIPTOR_HANDLE offscreenRTSrvCPUDescriptorHandle =
        m_graphicsDevice->getCPUDescriptorHandleAtIndex(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                                                        m_offscreenRTHeapIndexSRV);

    m_graphicsDevice->m_device->CreateShaderResourceView(m_offscreenRT.Get(), &srvDesc,
                                                         offscreenRTSrvCPUDescriptorHandle);

    // Setup ImGui.
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    ImGui::StyleColorsDark();

    // ImGui will utilize the last CPU/GPU descriptor handle.
    const D3D12_GPU_DESCRIPTOR_HANDLE imguiSrvGPUDescriptorHandle =
        m_graphicsDevice->getGPUDescriptorHandleAtIndex(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_imguiHeapIndexSRV);

    const D3D12_CPU_DESCRIPTOR_HANDLE imguiSrvCPUDescriptorHandle =
        m_graphicsDevice->getCPUDescriptorHandleAtIndex(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_imguiHeapIndexSRV);

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForD3D(m_window);
    ImGui_ImplDX12_Init(m_graphicsDevice->m_device.Get(), GraphicsDevice::FRAMES_IN_FLIGHT, DXGI_FORMAT_R8G8B8A8_UNORM,
                        m_graphicsDevice->m_cbvSrvUavDescriptorHeap.Get(), imguiSrvCPUDescriptorHandle,
                        imguiSrvGPUDescriptorHandle);
}

Engine::~Engine()
{
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

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
                ImGui_ImplSDL2_ProcessEvent(&event);

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
            const float deltaTime = static_cast<float>(
                std::chrono::duration_cast<std::chrono::milliseconds>(currentFrameTimePoint - previousFrameTimePoint)
                    .count());
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
    m_globalCBufferData.screenDimensions = DirectX::XMFLOAT2{
        (float)m_windowWidth,
        (float)m_windowHeight,
    };

    memcpy(m_globalCBufferPtr, reinterpret_cast<void *>(&m_globalCBufferData), sizeof(GlobalConstantBuffer));
}

void Engine::render()
{
    // Reset the command list and associated command allocator for this frame.
    const uint32_t currentFrameIndex = m_graphicsDevice->m_currentFrameIndex;
    auto &commandAllocator = m_graphicsDevice->m_commandAllocators[currentFrameIndex];
    auto &commandList = m_graphicsDevice->m_commandList;

    commandAllocator->Reset();
    utils::dxCheck(commandList->Reset(commandAllocator.Get(), nullptr));

    // Transition backbuffer from present to copy dest.
    const D3D12_RESOURCE_BARRIER presentToCopyDestBarrier = {
        .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
        .Transition =
            {
                .pResource = m_graphicsDevice->m_rtvBackBufferResources[currentFrameIndex].Get(),
                .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                .StateBefore = D3D12_RESOURCE_STATE_PRESENT,
                .StateAfter = D3D12_RESOURCE_STATE_COPY_DEST,
            },
    };

    // Transition compute shader output texture from copy src to unordered access.
    const D3D12_RESOURCE_BARRIER copySrvToUavBarrier = {
        .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
        .Transition =
            {
                .pResource = m_computeShaderOutputTexture.Get(),
                .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                .StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE,
                .StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            },
    };

    // Transition offscreen render target to RT from All_shader_resource.
    const D3D12_RESOURCE_BARRIER shaderResourceToRenderTargetBarrier = {
        .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
        .Transition =
            {
                .pResource = m_offscreenRT.Get(),
                .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                .StateBefore = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
                .StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET,
            },
    };

    const std::array<D3D12_RESOURCE_BARRIER, 3u> initialBatchedResourceBarriers = {
        presentToCopyDestBarrier,
        copySrvToUavBarrier,
        shaderResourceToRenderTargetBarrier,
    };

    commandList->ResourceBarrier(static_cast<uint32_t>(initialBatchedResourceBarriers.size()),
                                 initialBatchedResourceBarriers.data());

    const D3D12_CPU_DESCRIPTOR_HANDLE offscreenRtDescriptorHandle =
        m_graphicsDevice->getCPUDescriptorHandleAtIndex(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, m_offscreenRTHeapIndexRTV);

    constexpr std::array<const float, 4> clearColor{0.0f, 0.0f, 0.0f, 1.0f};
    commandList->ClearRenderTargetView(offscreenRtDescriptorHandle, clearColor.data(), 0u, nullptr);

    const std::array<ID3D12DescriptorHeap *, 1u> shaderVisibleDescriptorHeaps = {
        m_graphicsDevice->m_cbvSrvUavDescriptorHeap.Get(),
    };
    commandList->SetDescriptorHeaps(static_cast<uint32_t>(shaderVisibleDescriptorHeaps.size()),
                                    shaderVisibleDescriptorHeaps.data());

    // Render Imgui.
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
   
    ImGui::SliderFloat3("Sphere Center", &m_globalCBufferData.sphere.center.x, -1.0f, 1.0f);
    ImGui::SliderFloat("Sphere Radius", &m_globalCBufferData.sphere.radius, 0.1f, 10.0f);
    ImGui::ColorPicker3("Sphere Color", &m_globalCBufferData.sphere.color.x);

    ImGui::Render();
    commandList->OMSetRenderTargets(1u, &offscreenRtDescriptorHandle, false, nullptr);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.Get());

    // Transition offscreen render target to all shader resource.
    const D3D12_RESOURCE_BARRIER renderTargetToAllShaderResource = {
        .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
        .Transition =
            {
                .pResource = m_offscreenRT.Get(),
                .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                .StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET,
                .StateAfter = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
            },
    };

    commandList->ResourceBarrier(1u, &renderTargetToAllShaderResource);

    // Dispatch Calls.
    // Set necessary state.
    commandList->SetComputeRootSignature(m_rootSignature.Get());
    commandList->SetPipelineState(m_pipelineState.Get());

    const D3D12_GPU_DESCRIPTOR_HANDLE computeShaderOutputGPUDescriptor =
        m_graphicsDevice->getGPUDescriptorHandleAtIndex(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                                                        m_computeShaderUAVHeapIndex);
    commandList->SetComputeRootDescriptorTable(0u, computeShaderOutputGPUDescriptor);

    const D3D12_GPU_DESCRIPTOR_HANDLE offscreenRTGPUDescriptor = m_graphicsDevice->getGPUDescriptorHandleAtIndex(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_offscreenRTHeapIndexSRV);

    commandList->SetComputeRootDescriptorTable(1u, offscreenRTGPUDescriptor);

    commandList->SetComputeRootConstantBufferView(2u, m_globalCBufferResource->GetGPUVirtualAddress());

    const uint32_t dispatchX = std::max<uint32_t>(m_windowWidth / 12u, 1u);
    const uint32_t dispatchY = std::max<uint32_t>(m_windowHeight / 8u, 1u);

    commandList->Dispatch(dispatchX, dispatchY, 1);

    // Transition compute shader output texture from unordered access to copy source.
    const D3D12_RESOURCE_BARRIER uavToCopySourceBarrier = {
        .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
        .Transition =
            {
                .pResource = m_computeShaderOutputTexture.Get(),
                .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                .StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                .StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE,
            },
    };

    const std::array<D3D12_RESOURCE_BARRIER, 1u> intermediateBatchedResourceBarriers = {
        uavToCopySourceBarrier,
    };

    commandList->ResourceBarrier(static_cast<uint32_t>(intermediateBatchedResourceBarriers.size()),
                                 intermediateBatchedResourceBarriers.data());

    // Copy the compute shader output to the back buffer.
    commandList->CopyResource(m_graphicsDevice->m_rtvBackBufferResources[currentFrameIndex].Get(),
                              m_computeShaderOutputTexture.Get());

    // Transition back buffer to present from copy dest.
    const D3D12_RESOURCE_BARRIER copyDestToPresentBarrier = {
        .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
        .Transition =
            {
                .pResource = m_graphicsDevice->m_rtvBackBufferResources[currentFrameIndex].Get(),
                .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                .StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
                .StateAfter = D3D12_RESOURCE_STATE_PRESENT,
            },
    };

    const std::array<D3D12_RESOURCE_BARRIER, 1u> finalBatchedResourceBarriers = {
        copyDestToPresentBarrier,
    };

    commandList->ResourceBarrier(static_cast<uint32_t>(finalBatchedResourceBarriers.size()),
                                 finalBatchedResourceBarriers.data());

    // Execute command list.
    utils::dxCheck(commandList->Close());
    std::array<ID3D12CommandList *, 1u> commandLists = {
        commandList.Get(),
    };

    m_graphicsDevice->m_directCommandQueue->ExecuteCommandLists(1u, commandLists.data());

    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();

    // Present to swapchain.
    utils::dxCheck(m_graphicsDevice->m_swapchain->Present(1u, 0u));

    m_graphicsDevice->m_frameFenceValues[currentFrameIndex] = m_graphicsDevice->signal();

    // Wait for next frame's resources to be out of reference.
    m_graphicsDevice->m_currentFrameIndex = m_graphicsDevice->m_swapchain->GetCurrentBackBufferIndex();
    m_graphicsDevice->waitForFenceValue(m_graphicsDevice->m_frameFenceValues[m_graphicsDevice->m_currentFrameIndex]);
}
} // namespace cpt
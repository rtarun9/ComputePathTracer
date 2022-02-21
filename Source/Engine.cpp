#include "Pch.hpp"

#include "Engine.hpp"
#include "Application.hpp"

namespace cpt
{
	Engine::Engine(Config& config)
		: m_Width{ config.width }, m_Height{ config.height }, m_Title{ config.title }
	{
		m_AspectRatio = static_cast<float>(m_Width) / static_cast<float>(m_Height);
	} 

	void Engine::OnInit()
	{
		InitRendererCore();
		LoadContent();
	}

	void Engine::OnUpdate()
	{
		// Deltatime calculation.
		m_CurrentTimePoint = m_Clock.now();
		m_DeltaTIme = (m_CurrentTimePoint - m_PreviousTimePoint).count() * 1e-9;
		m_PreviousTimePoint = m_CurrentTimePoint;

		// Camera position calculations.
		float speed = m_DeltaTIme * m_CameraSpeed;

		if (m_Keys[static_cast<int>(Keys::Left)])
		{
			m_ConstantBufferData.cameraPosition.x -= speed;
		}
		else if (m_Keys[static_cast<int>(Keys::Right)])
		{
			m_ConstantBufferData.cameraPosition.x += speed;
		}

		if (m_Keys[static_cast<int>(Keys::Forward)])
		{
			m_ConstantBufferData.cameraPosition.z -= speed;
		}
		else if (m_Keys[static_cast<int>(Keys::Backward)])
		{
			m_ConstantBufferData.cameraPosition.z += speed;
		}

		// TODO : Map once, unmap at the end of apaplication if this approach is still being used.
		D3D11_MAPPED_SUBRESOURCE mappedSubresource{};
		ThrowIfFailed(m_DeviceContext->Map(m_ConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubresource));
		void* data = static_cast<void*>(&m_ConstantBufferData);
		memcpy(mappedSubresource.pData, data, sizeof(ConstantBufferData));
		m_DeviceContext->Unmap(m_ConstantBuffer.Get(), 0);
	}

	void Engine::OnCompute()
	{
		m_DeviceContext->CSSetUnorderedAccessViews(0u, 1u, m_UAV.GetAddressOf(), 0u);
		m_DeviceContext->CSSetConstantBuffers(0u, 1u, m_ConstantBuffer.GetAddressOf());
		m_DeviceContext->CSSetShader(m_ComputeShader.Get(), nullptr, 0u);
	
		m_DeviceContext->Dispatch(m_Width / 8, m_Height / 4, 1);
		Present();
	}

	void Engine::OnDestroy()
	{

	}

	// Needs to be rewritten using some sort of input mapping.
	void Engine::OnKeyDown(uint8_t keycode)
	{
		switch (keycode)
		{
		case 'A':
		{
			m_Keys[static_cast<size_t>(Keys::Left)] = true;
			break;
		}

		case 'D':
		{
			m_Keys[static_cast<size_t>(Keys::Right)] = true;
			break;
		}

		case 'W':
		{
			m_Keys[static_cast<size_t>(Keys::Forward)] = true;
			break;
		}

		case 'S':
		{
			m_Keys[static_cast<size_t>(Keys::Backward)] = true;
			break;
		}
		}
	}

	void Engine::OnKeyUp(uint8_t keycode)
	{
		switch (keycode)
		{
		case 'A':
		{
			m_Keys[static_cast<size_t>(Keys::Left)] = false;
			break;
		}

		case 'D':
		{
			m_Keys[static_cast<size_t>(Keys::Right)] = false;
			break;
		}

		case 'W':
		{
			m_Keys[static_cast<size_t>(Keys::Forward)] = false;
			break;
		}

		case 'S':
		{
			m_Keys[static_cast<size_t>(Keys::Backward)] = false;
			break;
		}
		}
	}

	uint32_t Engine::GetWidth() const
	{
		return m_Width;
	}

	uint32_t Engine::GetHeight() const
	{
		return m_Height;
	}

	std::wstring Engine::GetTitle() const
	{
		return m_Title;
	}

	void Engine::InitRendererCore()
	{
		DXGI_SWAP_CHAIN_DESC swapChainDesc
		{
			.BufferDesc
			{
				.Width = m_Width,
				.Height = m_Height,
				.RefreshRate
				{
					.Numerator = 60,
					.Denominator = 1,
				},
				.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
				.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
				.Scaling = DXGI_MODE_SCALING_UNSPECIFIED,
			},
			.SampleDesc
			{
				.Count = 1,
				.Quality = 0,
			},
			.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_UNORDERED_ACCESS | DXGI_USAGE_SHADER_INPUT,
			.BufferCount = Engine::NUMBER_OF_FRAMES,
			.OutputWindow = Application::GetWindowHandle(),
			.Windowed = TRUE,
			.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
			.Flags = 0
		};

		swapChainDesc.Flags = 0;
		UINT createDeviceFlags = 0;

#ifdef _DEBUG
		createDeviceFlags = D3D11_CREATE_DEVICE_DEBUG;
#endif

		ThrowIfFailed(::D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
			nullptr, 0, D3D11_SDK_VERSION, &swapChainDesc, &m_SwapChain, &m_Device, nullptr, &m_DeviceContext));

#ifdef _DEBUG
		ThrowIfFailed(m_Device.As(&m_Debug));
		ThrowIfFailed(m_Debug.As(&m_InfoQueue));

		m_InfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_WARNING, true);
		m_InfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);
		m_InfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
#endif

		wrl::ComPtr<ID3D11Texture2D> backBuffer;
		ThrowIfFailed(m_SwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)));

		ThrowIfFailed(m_Device->CreateUnorderedAccessView(backBuffer.Get(), nullptr, &m_UAV));
	}

	void Engine::LoadContent()
	{
		D3D11_BUFFER_DESC cbufferDesc
		{
			.ByteWidth = sizeof(ConstantBufferData),
			.Usage = D3D11_USAGE_DYNAMIC,
			.BindFlags = D3D11_BIND_CONSTANT_BUFFER,
			.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
			.MiscFlags = 0u,
			.StructureByteStride = 0u
		};

		ThrowIfFailed(m_Device->CreateBuffer(&cbufferDesc, nullptr, &m_ConstantBuffer));

		wrl::ComPtr<ID3DBlob> shaderBlob;
		wrl::ComPtr<ID3DBlob> errorBlob;

		HRESULT hr = D3DCompileFromFile(L"../Shaders/RayTracerCS.hlsl", nullptr, nullptr, "CsMain", "cs_5_0", 0, 0, &shaderBlob, &errorBlob);
		if (FAILED(hr))
		{
			const char* errorMessage = (const char*)errorBlob->GetBufferPointer();
			OutputDebugStringA(errorMessage);
		}

		ThrowIfFailed(m_Device->CreateComputeShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), nullptr, &m_ComputeShader));

		m_ConstantBufferData.screenDimension = dx::XMFLOAT2(m_Width, m_Height);
	}

	void Engine::Present()
	{
		m_SwapChain->Present(0u, 0u);

		m_FrameIndex++;
	}
}
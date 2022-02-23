#pragma once

#include "Pch.hpp"

namespace cpt
{
	struct Config
	{
		std::wstring title{};
		uint32_t width{};
		uint32_t height{};
	};

	struct ConstantBufferData
	{
		DirectX::XMFLOAT4 cameraPosition{DirectX::XMFLOAT4(0.0f, 0.5f, -1.0f, 1.0f)};
		DirectX::XMFLOAT4 cameraLookAt{DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)};
		DirectX::XMFLOAT2 screenDimension{};
		uint32_t frameIndex;
		DirectX::XMFLOAT4 padding;
		DirectX::XMFLOAT4 padding2;
		float padding3;
	};

	// Currently, Keys and INPUT_MAP is unused.
	enum class Keys : uint8_t
	{
		Left,
		Right,
		Forward,
		Backward,
		TotalKeys
	};

	class Engine
	{
	public:
		Engine(Config& config);
		~Engine() = default;

		void OnInit();
		void OnUpdate();
		void OnCompute();
		void OnDestroy();
		
		void OnKeyDown(uint8_t keycode);
		void OnKeyUp(uint8_t keycode);

		uint32_t GetWidth() const;
		uint32_t GetHeight() const;
		std::wstring GetTitle() const;

	private:
		void InitRendererCore();
		void LoadContent();

		void Present();

	private:
		// App variables.
		static constexpr uint32_t NUMBER_OF_FRAMES = 3;

		uint32_t m_Width{};
		uint32_t m_Height{};
		std::wstring m_Title{};

		float m_AspectRatio{};

		uint64_t m_FrameIndex{};

		// Core D3D and DXGI Objects.
		Microsoft::WRL::ComPtr<ID3D11Device> m_Device;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_DeviceContext;
		Microsoft::WRL::ComPtr<IDXGISwapChain> m_SwapChain;

		Microsoft::WRL::ComPtr<ID3D11Debug> m_Debug;
		Microsoft::WRL::ComPtr<ID3D11InfoQueue> m_InfoQueue;

		// Timing related objects.
		std::chrono::high_resolution_clock m_Clock;
		std::chrono::high_resolution_clock::time_point m_PreviousTimePoint;
		std::chrono::high_resolution_clock::time_point m_CurrentTimePoint;
		double m_DeltaTIme{};

		// Application data.
		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_UAV;
		
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_ConstantBuffer;
		Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_ComputeShader;

		ConstantBufferData m_ConstantBufferData{};

		// TODO : Put this data in seperate camera class.
		float m_CameraSpeed{ 2.5f };
		std::array<bool, static_cast<size_t>(Keys::TotalKeys)> m_Keys{};
	};
}
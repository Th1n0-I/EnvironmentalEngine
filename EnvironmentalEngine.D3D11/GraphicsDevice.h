#pragma once
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <stdexcept>

namespace EnvironmentalEngine {
	class GraphicsDevice {
	public:
		GraphicsDevice(HWND hwnd, int width, int height);
		~GraphicsDevice() = default;

		GraphicsDevice(const GraphicsDevice&) = delete;
		GraphicsDevice& operator = (const GraphicsDevice&) = delete;

		void Resize(int width, int height);
		void Present(bool vSync);

		ID3D11Device* Device() const { return m_device.Get(); }
		ID3D11DeviceContext* Context() const { return m_context.Get(); }
		ID3D11RenderTargetView* BackBufferRTV() const { return m_rtv.Get(); }
		ID3D11RenderTargetView* const* BackBufferRTVAddress() const { return m_rtv.GetAddressOf(); }

		const D3D11_VIEWPORT& BackBufferViewPort() const { return m_viewport; }
		int Width() const { return m_width; }
		int Height() const { return m_height; }
		float AspectRatio() const { return static_cast<float>(m_width) / m_height; }
		
	private:
		void CreateBackBufferView();

		Microsoft::WRL::ComPtr<ID3D11Device> m_device;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
		Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_rtv;
		D3D11_VIEWPORT m_viewport;
		int m_width = 0; int m_height = 0;
	};

	inline void Check(HRESULT hr)
	{
		if (FAILED(hr))
		{
			throw std::runtime_error("D3D11 Call failed!");
		}
	}

}
#include "pch.h"
#include "GraphicsDevice.h"

namespace EnvironmentalEngine {
	void GraphicsDevice::CreateBackBufferView() {
		Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
		Check(m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)));
		Check(m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_rtv));

		D3D11_VIEWPORT vp = {};
		vp.Width = static_cast<float>(m_width);
		vp.Height = static_cast<float>(m_height);
		vp.MaxDepth = 1.0f;
		m_viewport = vp;
		m_context->RSSetViewports(1, &m_viewport);
	}

	void GraphicsDevice::Resize(int width, int height) {

		m_context->OMSetRenderTargets(0, nullptr, nullptr);
		m_rtv.Reset();
		m_context->Flush();
		Check(m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0));

		m_width = width; m_height = height;
		CreateBackBufferView();
		
	}

	void GraphicsDevice::Present(bool vSync) {
		m_swapChain->Present(vSync ? 1 : 0, 0);
	}

	GraphicsDevice::GraphicsDevice(HWND hwnd, int width, int height) : m_width(width), m_height(height) {

		DXGI_SWAP_CHAIN_DESC desc = {};
		desc.BufferDesc.Width = width;
		desc.BufferDesc.Height = height;
		desc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		desc.BufferCount = 2;
		desc.OutputWindow = hwnd;
		desc.Windowed = true;
		desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

		UINT flags = 0;
		#ifdef _DEBUG
			flags |= D3D11_CREATE_DEVICE_DEBUG;
		#endif

		Check(D3D11CreateDeviceAndSwapChain(
			nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			nullptr,
			flags,
			nullptr, 0,
			D3D11_SDK_VERSION,
			&desc,
			&m_swapChain,
			&m_device,
			nullptr,
			&m_context
		));

		CreateBackBufferView();
	}
}
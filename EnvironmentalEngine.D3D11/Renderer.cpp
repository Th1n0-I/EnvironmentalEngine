#include "pch.h"
#include "Renderer.h"
#include <stdexcept>


#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;

inline void Check(HRESULT hr) {
	if (FAILED(hr)) {
		throw std::runtime_error("D3D11 Call failed!");
	}
}

namespace EnvironmentalEngine{
	Renderer::Renderer(HWND hwnd, int width, int height) {
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

		ComPtr<ID3D11Texture2D> backBuffer;
		Check(m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)));
		Check(m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_rtv));

		D3D11_VIEWPORT vp = {};
		vp.Width = static_cast<float>(width);
		vp.Height = static_cast<float>(height);
		vp.MaxDepth = 1.0f;
		m_context->RSSetViewports(1, &vp);

	}

	void Renderer::BeginFrame() {
		const float clear[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
		m_context->OMSetRenderTargets(1, m_rtv.GetAddressOf(), nullptr);
		m_context->ClearRenderTargetView(m_rtv.Get(), clear);
	}

	void Renderer::EndFrame() {
		m_swapChain->Present(1, 0);
	}
}
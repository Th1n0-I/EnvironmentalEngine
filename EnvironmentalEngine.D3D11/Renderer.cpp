#include "pch.h"
#include "Renderer.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"
#include <stdexcept>
#include <d3dcompiler.h>
#include <cmath>
#include <DirectXMath.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;
using namespace DirectX;

int old_width = 0;
int old_height = 0;
float aspect_ratio = 0.0f;

inline void Check(HRESULT hr) 
{
	if (FAILED(hr)) 
	    {
		throw std::runtime_error("D3D11 Call failed!");
	}
}

struct FrameConstants {
	XMFLOAT4X4 transform;
};

struct Vertex
{
    float x, y, z, r, g, b;
};

std::wstring ExeDir()
{
	wchar_t path[MAX_PATH];
	GetModuleFileNameW(nullptr, path, MAX_PATH);
	std::wstring p(path);
	return p.substr(0, p.find_last_of(L"\\/") + 1);
}

ComPtr<ID3DBlob> LoadShaderByteCode(
    const wchar_t* filename, const char* entry, const char* target)
{
    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    
    ComPtr<ID3DBlob> code;
    ComPtr<ID3DBlob> error;
    
    HRESULT hr = D3DCompileFromFile(
        filename, nullptr, nullptr,
        entry, target,
        flags, 0,
        &code, &error);
    
	if (FAILED(hr)) {
		std::string msg = "Shader compile failed!";
		if (error)
			msg += std::string(": ") + static_cast<const char*>(error->GetBufferPointer());
		OutputDebugStringA(msg.c_str());
		throw std::runtime_error(msg);
	}
    
    return code;
}

namespace EnvironmentalEngine{
	void Renderer::Resize(int width, int height)
	{
		if (width == 0 || height == 0) return;
		aspect_ratio = static_cast<float>(width) / static_cast<float>(height);
		m_rtv.Reset();

		m_depthView.Reset();
		m_depthTex.Reset();

		Check(m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0));

		ComPtr<ID3D11Texture2D> backBuffer;
		Check(m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)));
		Check(m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_rtv));

		D3D11_TEXTURE2D_DESC dd = {};
		dd.Width = width;
		dd.Height = height;
		dd.MipLevels = 1;
		dd.ArraySize = 1;
		dd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dd.SampleDesc.Count = 1;
		dd.Usage = D3D11_USAGE_DEFAULT;
		dd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		Check(m_device->CreateTexture2D(&dd, nullptr, &m_depthTex));
		Check(m_device->CreateDepthStencilView(m_depthTex.Get(), nullptr, &m_depthView));

		D3D11_VIEWPORT vp = {};
		vp.Width = static_cast<float>(width);
		vp.Height = static_cast<float>(height);
		vp.MaxDepth = 1.0f;
		m_context->RSSetViewports(1, &vp);
	}

	Renderer::Renderer(HWND hwnd, int width, int height) 
    {
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

		D3D11_TEXTURE2D_DESC dd = {};
		dd.Width = width;
		dd.Height = height;
		dd.MipLevels = 1;
		dd.ArraySize = 1;
		dd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dd.SampleDesc.Count = 1;
		dd.Usage = D3D11_USAGE_DEFAULT;
		dd.BindFlags = D3D11_BIND_DEPTH_STENCIL;

		Check(m_device->CreateTexture2D(&dd, nullptr, &m_depthTex));
		Check(m_device->CreateDepthStencilView(m_depthTex.Get(), nullptr, &m_depthView));
        
	    CreateCube();

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();
		ImGui_ImplWin32_Init(hwnd);
		ImGui_ImplDX11_Init(m_device.Get(), m_context.Get());
	}

	Renderer::~Renderer()
	{
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}

	void Renderer::BeginFrame(int width, int height, float deltaTime, const DirectX::XMMATRIX& view) 
    {
		static float pitch = 0.0f;
		static float yaw = 0.0f;

		if ((width != old_width || height != old_height) && width != 0 && height != 0) {
			Resize(width, height);
			old_width = width;
			old_height = height;
		}

		const float clear[4] = { 0.39f, 0.58f, 0.93f, 1.0f };
		m_context->OMSetRenderTargets(1, m_rtv.GetAddressOf(), m_depthView.Get());
		m_context->ClearRenderTargetView(m_rtv.Get(), clear);
		m_context->ClearDepthStencilView(m_depthView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("Triangle Rotation");
		ImGui::SliderFloat("Rotation Speed", &m_spinSpeed, 0.0f, 10.0f);
		ImGui::SliderFloat("FOV", &m_fov, 1.0f, 179.0f);
		ImGui::SliderFloat("Pitch", &pitch, -90.0f, 90.0f);
		ImGui::SliderFloat("Yaw", &yaw, -180.0f, 180.0f);
		ImGui::End();

		float radPitch = XMConvertToRadians(pitch);
		float radYaw = XMConvertToRadians(yaw);
		
		static float angle = 0.0f;

		angle += deltaTime * m_spinSpeed;

		XMMATRIX world = XMMatrixRotationY(angle);

		XMMATRIX proj = XMMatrixPerspectiveFovLH(
			XMConvertToRadians(m_fov),
			aspect_ratio,
			0.1f,
			1000.0f);
		XMMATRIX finalMatrix = world * view * proj;

		FrameConstants constants = {};
		XMStoreFloat4x4(&constants.transform, XMMatrixTranspose(finalMatrix));

		D3D11_MAPPED_SUBRESOURCE mapped = {};
		m_context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		memcpy(mapped.pData, &constants, sizeof(constants));
		m_context->Unmap(m_constantBuffer.Get(), 0);

		m_context->VSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());

		UINT stride = sizeof(Vertex);
		UINT offset = 0;

		m_context->IASetInputLayout(m_inputLayout.Get());
		m_context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
		m_context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
		m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
		m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);

		m_context->DrawIndexed(m_indexCount, 0, 0);
	}

	void Renderer::EndFrame() 
    {
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		m_swapChain->Present(1, 0);
	}

    void Renderer::CreateCube()
    {
		Vertex vertices[] =
		{//     x      y      z          r     g     b
			{ -0.5f, -0.5f, -0.5f,		1.0f, 0.0f, 0.0f },
			{  0.5f, -0.5f, -0.5f,		0.0f, 1.0f, 0.0f },
			{  0.5f,  0.5f, -0.5f,		0.0f, 0.0f, 1.0f },
			{ -0.5f,  0.5f, -0.5f,		1.0f, 1.0f, 0.0f },
			{ -0.5f, -0.5f,  0.5f,		1.0f, 0.0f, 1.0f },
			{  0.5f, -0.5f,  0.5f,		0.0f, 1.0f, 1.0f },
			{  0.5f,  0.5f,  0.5f,		1.0f, 1.0f, 1.0f },
			{ -0.5f,  0.5f,  0.5f,		0.2f, 0.2f, 0.2f },
		};

		unsigned int indices[] = {
			0, 2, 1,	0, 3, 2,
			4, 5, 6,	4, 6, 7,
			4, 3, 0,	4, 7, 3,
			1, 6, 5,	1, 2, 6,
			3, 6, 2,	3, 7, 6,
			4, 1, 5,	4, 0, 1
		};

		D3D11_BUFFER_DESC ibd = {};
		ibd.ByteWidth = sizeof(indices);
		ibd.Usage = D3D11_USAGE_DEFAULT;
		ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;

		D3D11_SUBRESOURCE_DATA iinit = {};
		iinit.pSysMem = indices;

		Check(m_device->CreateBuffer(&ibd, &iinit, &m_indexBuffer));
	    
	    D3D11_BUFFER_DESC bd = {};
	    bd.ByteWidth = sizeof(vertices);
	    bd.Usage = D3D11_USAGE_DEFAULT;
	    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	    
	    D3D11_SUBRESOURCE_DATA init = {};
	    init.pSysMem = vertices;

		Check(m_device->CreateBuffer(&bd, &init, &m_vertexBuffer));

		m_indexCount = sizeof(indices) / sizeof(indices[0]);

		D3D11_BUFFER_DESC cbd = {};
		cbd.ByteWidth = sizeof(FrameConstants);
		cbd.Usage = D3D11_USAGE_DYNAMIC;
		cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		Check(m_device->CreateBuffer(&cbd, nullptr, &m_constantBuffer));

		std::wstring shaderPath = ExeDir() + L"Triangle.hlsl";

		auto vsBlob = LoadShaderByteCode((shaderPath).c_str(), "VSMain", "vs_5_0");
		auto psBlob = LoadShaderByteCode((shaderPath).c_str(), "PSMain", "ps_5_0");

		Check(m_device->CreateVertexShader(
			vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
			nullptr, &m_vertexShader
		));
		
		Check(m_device->CreatePixelShader(
			psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
			nullptr, &m_pixelShader
		));

		D3D11_INPUT_ELEMENT_DESC layout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
			
		Check(m_device->CreateInputLayout(
			layout, 2,
			vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
			&m_inputLayout
		));
    }
}

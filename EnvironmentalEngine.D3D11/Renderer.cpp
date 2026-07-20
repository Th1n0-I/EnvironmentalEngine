#include "pch.h"
#include "Renderer.h"
#include "Mesh.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"
#include <stdexcept>
#include <d3dcompiler.h>
#include <cmath>
#include <DirectXMath.h>
#include <Lights.h>
#include <memory>

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

struct PerFrameConstants {
	
	XMFLOAT3 camPos;
	float padding0;
	XMFLOAT3 ambientColor;
	float ambientIntensity;
	XMFLOAT3 lightColor;
	float padding1;
	XMFLOAT3 lightDirection;
	float padding2;
	XMFLOAT3 pLightPosition;
	float pIntensity;
	XMFLOAT3 pColor;
	float padding;
};

static_assert(sizeof(PerFrameConstants) % 16 == 0, "PerFrameConstants is the wrong size");

struct PerObjectConstants {
	XMFLOAT4X4 transform;
	XMFLOAT4X4 world;
	XMFLOAT4X4 normal;
	XMFLOAT4 cubeColor;
	float specularIntensity;
	float smoothness;
	float padding[2];
};

static_assert(sizeof(PerObjectConstants) % 16 == 0, "PerObjectConstants is the wrong size");

struct Vertex
{
    float x, y, z, nx, ny, nz;
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

	void Renderer::BeginFrame(int width, int height, float deltaTime, const DirectX::XMMATRIX& view, DirectX::XMFLOAT3 camPos, DirectionalLight& dl, AmbientLight& al, PointLight& pl) 
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

		ImGui::Begin("Environmental Engine");
	

		

		m_viewMatrix = view;
		m_projMatrix = XMMatrixPerspectiveFovLH(
			XMConvertToRadians(m_fov),
			aspect_ratio,
			0.1f,
			1000.0f);

		PerFrameConstants frameConstants = {};
		XMStoreFloat3(&frameConstants.camPos, XMVectorSet(camPos.x, camPos.y, camPos.z, 0.0f));

		XMStoreFloat3(&frameConstants.ambientColor, XMVectorSet(al.color.x, al.color.y, al.color.z, 0.0f));
		XMStoreFloat(&frameConstants.ambientIntensity, XMVectorSet(al.intensity, 0.0f, 0.0f, 0.0f));
		
		XMStoreFloat3(&frameConstants.lightColor, XMVectorSet(dl.color.x, dl.color.y, dl.color.z, 0.0f));
		XMStoreFloat3(&frameConstants.lightDirection, XMVectorSet(dl.direction.x, dl.direction.y, dl.direction.z, 0.0f));

		XMStoreFloat3(&frameConstants.pLightPosition, XMVectorSet(pl.position.x, pl.position.y, pl.position.z, 0.0f));
		XMStoreFloat3(&frameConstants.pColor, XMVectorSet(pl.color.x, pl.color.y, pl.color.z, 0.0f));
		XMStoreFloat(&frameConstants.pIntensity, XMVectorSet(pl.intensity, 0.0f, 0.0f, 0.0f));

		D3D11_MAPPED_SUBRESOURCE mapped = {};
		m_context->Map(m_perFrameBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		memcpy(mapped.pData, &frameConstants, sizeof(frameConstants));
		m_context->Unmap(m_perFrameBuffer.Get(), 0);

		m_context->VSSetConstantBuffers(0, 1, m_perFrameBuffer.GetAddressOf());
		m_context->PSSetConstantBuffers(0, 1, m_perFrameBuffer.GetAddressOf());
		m_context->VSSetConstantBuffers(1, 1, m_perObjectBuffer.GetAddressOf());
		m_context->PSSetConstantBuffers(1, 1, m_perObjectBuffer.GetAddressOf());

		m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
		m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);

		m_context->IASetInputLayout(m_inputLayout.Get());
	}

	void Renderer::Draw(const MeshRenderer& mr) 
	{
		XMMATRIX world =
			XMMatrixScaling(mr.scale.x, mr.scale.y, mr.scale.z) *
			XMMatrixRotationRollPitchYaw(DirectX::XMConvertToRadians(mr.rotation.x), DirectX::XMConvertToRadians(mr.rotation.y), DirectX::XMConvertToRadians(mr.rotation.z)) *
			XMMatrixTranslation(mr.position.x, mr.position.y, mr.position.z);

		XMMATRIX final = world * m_viewMatrix * m_projMatrix;

		XMMATRIX normal = XMMatrixInverse(nullptr, world);

		PerObjectConstants constants = {};
		XMStoreFloat4x4(&constants.transform, XMMatrixTranspose(final));
		XMStoreFloat4x4(&constants.world, XMMatrixTranspose(world));
		XMStoreFloat4x4(&constants.normal, normal);

		XMStoreFloat4(&constants.cubeColor, XMVectorSet(mr.color.x, mr.color.y, mr.color.z, 0.0f));
		XMStoreFloat(&constants.specularIntensity, XMVectorSet(mr.specularIntensity, 0.0f, 0.0f, 0.0f));
		XMStoreFloat(&constants.smoothness, XMVectorSet(mr.smoothness, 0.0f, 0.0f, 0.0f));

		D3D11_MAPPED_SUBRESOURCE mapped = {};
		m_context->Map(m_perObjectBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		memcpy(mapped.pData, &constants, sizeof(constants));
		m_context->Unmap(m_perObjectBuffer.Get(), 0);

		m_cubeMesh->Bind(m_context.Get());
		m_context->DrawIndexed(m_cubeMesh->IndexCount(), 0, 0);
	}

	void Renderer::EndFrame() 
    {
		ImGui::End();
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		m_swapChain->Present(0, 0);
	}

	void Renderer::CreateCube()
    {
		Vertex vertices[] =
		{//     x      y      z         nx     ny     nz
			{ -0.5f, -0.5f, -0.5f,	  -1.0f,  0.0f,  0.0f }, //0
			{ -0.5f, -0.5f, -0.5f, 	   0.0f, -1.0f,  0.0f }, //1
			{ -0.5f, -0.5f, -0.5f,     0.0f,  0.0f, -1.0f }, //2
			{  0.5f, -0.5f, -0.5f,	   1.0f,  0.0f,  0.0f }, //3
			{  0.5f, -0.5f, -0.5f,	   0.0f, -1.0f,  0.0f }, //4
			{  0.5f, -0.5f, -0.5f,	   0.0f,  0.0f, -1.0f }, //5
			{  0.5f,  0.5f, -0.5f,	   1.0f,  0.0f,  0.0f }, //6
			{  0.5f,  0.5f, -0.5f,	   0.0f,  1.0f,  0.0f }, //7
			{  0.5f,  0.5f, -0.5f,	   0.0f,  0.0f, -1.0f }, //8
			{ -0.5f,  0.5f, -0.5f,	  -1.0f,  0.0f,  0.0f }, //9
			{ -0.5f,  0.5f, -0.5f,	   0.0f,  1.0f,  0.0f }, //10
			{ -0.5f,  0.5f, -0.5f,	   0.0f,  0.0f, -1.0f }, //11
			{ -0.5f, -0.5f,  0.5f,    -1.0f,  0.0f,  0.0f }, //12 
			{ -0.5f, -0.5f,  0.5f,	   0.0f, -1.0f,  0.0f }, //13
			{ -0.5f, -0.5f,  0.5f,	   0.0f,  0.0f,  1.0f }, //14
			{  0.5f, -0.5f,  0.5f,	   1.0f,  0.0f,  0.0f }, //15
			{  0.5f, -0.5f,  0.5f,	   0.0f, -1.0f,  0.0f }, //16
			{  0.5f, -0.5f,  0.5f,	   0.0f,  0.0f,  1.0f }, //17
			{  0.5f,  0.5f,  0.5f,	   1.0f,  0.0f,  0.0f }, //18
			{  0.5f,  0.5f,  0.5f,	   0.0f,  1.0f,  0.0f }, //19
			{  0.5f,  0.5f,  0.5f,     0.0f,  0.0f,  1.0f }, //20
			{ -0.5f,  0.5f,  0.5f,    -1.0f,  0.0f,  0.0f }, //21
			{ -0.5f,  0.5f,  0.5f,	   0.0f,  1.0f,  0.0f }, //22
			{ -0.5f,  0.5f,  0.5f,	   0.0f,  0.0f,  1.0f }, //23
		};

		unsigned int indices[] =
		{
			14, 17, 20,		14, 20, 23,
			 2,  8,  5,		 2, 11,  8,
			 0, 12, 21,		 0, 21,  9,
			 3, 18, 15,		 3,  6, 18,
			 1, 16, 13,		 1,  4, 16,
			10, 19,  7,		10, 22, 19,
		};

		m_cubeMesh = std::make_unique<Mesh>(m_device.Get(), vertices, sizeof(vertices) / sizeof(vertices[0]), indices, sizeof(indices) / sizeof(indices[0]));

		D3D11_BUFFER_DESC pfcbd = {};
		pfcbd.ByteWidth = sizeof(PerFrameConstants);
		pfcbd.Usage = D3D11_USAGE_DYNAMIC;
		pfcbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		pfcbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		Check(m_device->CreateBuffer(&pfcbd, nullptr, &m_perFrameBuffer));

		D3D11_BUFFER_DESC pocbd = {};
		pocbd.ByteWidth = sizeof(PerObjectConstants);
		pocbd.Usage = D3D11_USAGE_DYNAMIC;
		pocbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		pocbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		
		Check(m_device->CreateBuffer(&pocbd, nullptr, &m_perObjectBuffer));

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
			{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
		};
			
		Check(m_device->CreateInputLayout(
			layout, 2,
			vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
			&m_inputLayout
		));
    }
}

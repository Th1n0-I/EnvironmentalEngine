#include "pch.h"
#include "Renderer.h"
#include "Mesh.h"
#include "GameObject.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"
#include "FastNoiseLite.h"
#include <stdexcept>
#include <d3dcompiler.h>
#include <cmath>
#include <DirectXMath.h>
#include <Lights.h>
#include <memory>
#include <vector>
#include <map>
#include <tuple>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

const static float PI = 3.14159265358987;

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

struct atmosphereConstants {
	XMFLOAT4X4 invViewProj;
	XMFLOAT3 camPos;
	float innerRadius;
	XMFLOAT3 planetCenter;
	float outerRadius;
	XMFLOAT3 dirToSun;
	float scaleHeight;
	XMFLOAT3 rayleighCoeff;
	float sunIntensity;
};

static_assert(sizeof(PerObjectConstants) % 16 == 0, "PerObjectConstants is the wrong size");

float clamp(float v, float minValue, float maxValue) {
	return max(min(v, maxValue), minValue);
}

float smoothstep(float edge0, float edge1, float x) {

	x = clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);

	return x * x * (3 - 2 * x);
}

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
		dd.Format = DXGI_FORMAT_R24G8_TYPELESS;
		dd.SampleDesc.Count = 1;
		dd.Usage = D3D11_USAGE_DEFAULT;
		dd.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

		D3D11_DEPTH_STENCIL_VIEW_DESC dsv = {};
		dsv.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsv.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		dsv.Texture2D.MipSlice = 0;
		dsv.Flags = 0;

		D3D11_SHADER_RESOURCE_VIEW_DESC srv = {};
		srv.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		srv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srv.Texture2D.MostDetailedMip = 0;
		srv.Texture2D.MipLevels = 1;

		Check(m_device->CreateTexture2D(&dd, nullptr, &m_depthTex));
		Check(m_device->CreateDepthStencilView(m_depthTex.Get(), &dsv, &m_depthView));
		Check(m_device->CreateShaderResourceView(m_depthTex.Get(), &srv, &m_depthSrv));

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
		dd.Format = DXGI_FORMAT_R24G8_TYPELESS;
		dd.SampleDesc.Count = 1;
		dd.Usage = D3D11_USAGE_DEFAULT;
		dd.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

		D3D11_DEPTH_STENCIL_VIEW_DESC dsv = {};
		dsv.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsv.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		dsv.Texture2D.MipSlice = 0;
		dsv.Flags = 0;

		D3D11_SHADER_RESOURCE_VIEW_DESC srv = {};
		srv.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		srv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srv.Texture2D.MostDetailedMip = 0;
		srv.Texture2D.MipLevels = 1;

		Check(m_device->CreateTexture2D(&dd, nullptr, &m_depthTex));
		Check(m_device->CreateDepthStencilView(m_depthTex.Get(), &dsv, &m_depthView));
		Check(m_device->CreateShaderResourceView(m_depthTex.Get(), &srv, &m_depthSrv));
        
		CreateCube();
		CreatePlanet(1, 248);

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();
		ImGui_ImplWin32_Init(hwnd);
		ImGui_ImplDX11_Init(m_device.Get(), m_context.Get());

		D3D11_BLEND_DESC bd = {};
		bd.RenderTarget[0].BlendEnable = TRUE;
		bd.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		bd.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

		Check(m_device->CreateBlendState(&bd, &m_additiveBlend));
	}

	Renderer::~Renderer()
	{
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}

	void Renderer::BeginFrame(int width, int height, float deltaTime, const DirectX::XMMATRIX& view, DirectX::XMFLOAT3 camPos, DirectionalLight& dl, AmbientLight& al, PointLight& pl) 
    {

		m_lightDir = dl.direction;

		if ((width != old_width || height != old_height) && width != 0 && height != 0) {
			Resize(width, height);
			old_width = width;
			old_height = height;
		}

		const float clear[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
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
			5000.0f);

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

	void Renderer::Draw(const MeshRenderer& mr, const Transform& tr) 
	{
		XMMATRIX world =
			XMMatrixScaling(tr.scale.x, tr.scale.y, tr.scale.z) *
			XMMatrixRotationRollPitchYaw(DirectX::XMConvertToRadians(tr.rotation.x), DirectX::XMConvertToRadians(tr.rotation.y), DirectX::XMConvertToRadians(tr.rotation.z)) *
			XMMatrixTranslation(tr.position.x, tr.position.y, tr.position.z);

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

		mr.mesh->Bind(m_context.Get());
		m_context->DrawIndexed(mr.mesh->IndexCount(), 0, 0);
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
		{//     x      y      z         nx     ny     nz  elevation
			{ -0.5f, -0.5f, -0.5f,	  -1.0f,  0.0f,  0.0f, -1.0f }, //0
			{ -0.5f, -0.5f, -0.5f, 	   0.0f, -1.0f,  0.0f, -1.0f }, //1
			{ -0.5f, -0.5f, -0.5f,     0.0f,  0.0f, -1.0f, -1.0f }, //2
			{  0.5f, -0.5f, -0.5f,	   1.0f,  0.0f,  0.0f, -1.0f }, //3
			{  0.5f, -0.5f, -0.5f,	   0.0f, -1.0f,  0.0f, -1.0f }, //4
			{  0.5f, -0.5f, -0.5f,	   0.0f,  0.0f, -1.0f, -1.0f }, //5
			{  0.5f,  0.5f, -0.5f,	   1.0f,  0.0f,  0.0f, -1.0f }, //6
			{  0.5f,  0.5f, -0.5f,	   0.0f,  1.0f,  0.0f, -1.0f }, //7
			{  0.5f,  0.5f, -0.5f,	   0.0f,  0.0f, -1.0f, -1.0f }, //8
			{ -0.5f,  0.5f, -0.5f,	  -1.0f,  0.0f,  0.0f, -1.0f }, //9
			{ -0.5f,  0.5f, -0.5f,	   0.0f,  1.0f,  0.0f, -1.0f }, //10
			{ -0.5f,  0.5f, -0.5f,	   0.0f,  0.0f, -1.0f, -1.0f }, //11
			{ -0.5f, -0.5f,  0.5f,    -1.0f,  0.0f,  0.0f, -1.0f }, //12 
			{ -0.5f, -0.5f,  0.5f,	   0.0f, -1.0f,  0.0f, -1.0f }, //13
			{ -0.5f, -0.5f,  0.5f,	   0.0f,  0.0f,  1.0f, -1.0f }, //14
			{  0.5f, -0.5f,  0.5f,	   1.0f,  0.0f,  0.0f, -1.0f }, //15
			{  0.5f, -0.5f,  0.5f,	   0.0f, -1.0f,  0.0f, -1.0f }, //16
			{  0.5f, -0.5f,  0.5f,	   0.0f,  0.0f,  1.0f, -1.0f }, //17
			{  0.5f,  0.5f,  0.5f,	   1.0f,  0.0f,  0.0f, -1.0f }, //18
			{  0.5f,  0.5f,  0.5f,	   0.0f,  1.0f,  0.0f, -1.0f }, //19
			{  0.5f,  0.5f,  0.5f,     0.0f,  0.0f,  1.0f, -1.0f }, //20
			{ -0.5f,  0.5f,  0.5f,    -1.0f,  0.0f,  0.0f, -1.0f }, //21
			{ -0.5f,  0.5f,  0.5f,	   0.0f,  1.0f,  0.0f, -1.0f }, //22
			{ -0.5f,  0.5f,  0.5f,	   0.0f,  0.0f,  1.0f, -1.0f }, //23
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

		std::vector<Vertex> sVertices;
		std::vector<UINT> sIndices;
		UINT stackAmount = 16;
		UINT sliceAmount = 16;
		float r = 1.0f;

		for (int stack = 0; stack <= stackAmount; stack++) {
			float phi = PI * static_cast<float>(stack) / static_cast<float>(stackAmount);
			for (int slice = 0; slice <= sliceAmount; slice++) {
				float theta = 2.0f * PI * static_cast<float>(slice) / static_cast<float>(sliceAmount);
				float x = r * sin(phi) * cos(theta);
				float y = r * cos(phi);
				float z = r * sin(phi) * sin(theta);
				sVertices.push_back({ x, y, z, x / r, y / r, z / r, -1.0f});
			}
		}

		for (int stack = 0; stack < stackAmount; stack++) {
			for (int slice = 0; slice < sliceAmount; slice++) {
				UINT a = stack * (sliceAmount + 1) + slice;
				UINT b = a + 1;
				UINT c = a + (sliceAmount + 1);
				UINT d = c + 1;

				sIndices.push_back(a); sIndices.push_back(b); sIndices.push_back(c); sIndices.push_back(b); sIndices.push_back(d); sIndices.push_back(c);
			}
		}

		m_sphereMesh = std::make_unique<Mesh>(m_device.Get(), sVertices.data(), (UINT)sVertices.size(), sIndices.data(), (UINT)sIndices.size());

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

		D3D11_BUFFER_DESC acbd = {};
		acbd.ByteWidth = sizeof(atmosphereConstants);
		acbd.Usage = D3D11_USAGE_DYNAMIC;
		acbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		acbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		Check(m_device->CreateBuffer(&acbd, nullptr, &m_atmosphereBuffer));

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
			{"ELEVATION", 0, DXGI_FORMAT_R32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
		};
			
		Check(m_device->CreateInputLayout(
			layout, 3,
			vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
			&m_inputLayout
		));

		std::wstring atmoPath = ExeDir() + L"Atmosphere.hlsl";
		auto avs = LoadShaderByteCode(atmoPath.c_str(), "VSMain", "vs_5_0");
		auto aps = LoadShaderByteCode(atmoPath.c_str(), "PSMain", "ps_5_0");


		Check(m_device->CreateVertexShader(
			avs->GetBufferPointer(), avs->GetBufferSize(),
			nullptr, &m_atmoVS
		));

		Check(m_device->CreatePixelShader(
			aps->GetBufferPointer(), aps->GetBufferSize(),
			nullptr, &m_atmoPS


		));
    }

	void Renderer::CreatePlanet(float radius, UINT res) {

		FastNoiseLite mtnN;
		mtnN.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
		mtnN.SetFractalType(FastNoiseLite::FractalType_Ridged);
		mtnN.SetFractalOctaves(5);
		mtnN.SetFrequency(1.8f);
		mtnN.SetFractalGain(0.5f);

		FastNoiseLite baseN;
		baseN.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
		baseN.SetFractalType(FastNoiseLite::FractalType_FBm);
		baseN.SetFractalOctaves(4);
		baseN.SetFrequency(1.0f);
		baseN.SetFractalGain(0.5f);

		std::vector<Vertex> vertices;
		std::vector<UINT> indices;
		
		XMFLOAT3 localUp[6] = { 
			{  0.0f,  1.0f,  0.0f }, 
			{  0.0f, -1.0f,  0.0f }, 
			{  1.0f,  0.0f,  0.0f }, 
			{ -1.0f,  0.0f,  0.0f }, 
			{  0.0f,  0.0f,  1.0f }, 
			{  0.0f,  0.0f, -1.0f } };
		XMFLOAT3 axisA[6] = {};
		XMFLOAT3 axisB[6] = {};

		for (int s = 0; s < 6; s++){
			axisA[s] = { localUp[s].y, localUp[s].z, localUp[s].x };
			XMVECTOR cross = XMVector3Cross(XMVectorSet(localUp[s].x, localUp[s].y, localUp[s].z, 0.0f), XMVectorSet(axisA[s].x, axisA[s].y, axisA[s].z, 0.0f));
			XMStoreFloat3(&axisB[s], cross);
			for (int x = 0; x < res; x++) {
				for (int y = 0; y < res; y++) {
					XMFLOAT2 percent = { x / (res - 1.0f), y / (res - 1.0f)};
					XMVECTOR cubePos = XMVectorSet(localUp[s].x, localUp[s].y, localUp[s].z, 0.0f) +
						(percent.x - 0.5f) * 2.0f * XMVectorSet(axisA[s].x, axisA[s].y, axisA[s].z, 0.0f) +
						(percent.y - 0.5f) * 2.0f * XMVectorSet(axisB[s].x, axisB[s].y, axisB[s].z, 0.0f);
					
					XMFLOAT3 spherePos;
					XMStoreFloat3(&spherePos, XMVector3Normalize(cubePos));
					float base = baseN.GetNoise(spherePos.x, spherePos.y, spherePos.z) * 0.5f + 0.5f;
					float mtn = mtnN.GetNoise(spherePos.x, spherePos.y, spherePos.z) * 0.5f + 0.5f;

					float sharpness = 2.0f;
					mtn = powf(mtn, sharpness);

					float mask = smoothstep(0.65f, 0.8f, base);
					float mtnStrength = 0.6f;
					float strength = 0.15f;
					float e = base + mtn * mask * mtnStrength;

					float seaLevel = 0.5f;
					float land = max(e - seaLevel, 0.0f);

					float h = radius * (1.0f + strength * land);
					

					vertices.push_back({ spherePos.x * h, spherePos.y * h, spherePos.z * h, 0.0f, 0.0f, 0.0f, e });
				}
			}
		}

		for (int s = 0; s < 6; s++) {
			for (int x = 0; x < res - 1; x++) {
				for (int y = 0; y < res - 1; y++) {
					int i = x + y * res + s * res * res;
					indices.push_back(i);
					indices.push_back(i + res);
					indices.push_back(i + res + 1);

					indices.push_back(i);
					indices.push_back(i + res + 1);
					indices.push_back(i + 1);
				}
			}
		}

		for (size_t t = 0; t < indices.size(); t+= 3) {
			UINT ia = indices[t], ib = indices[t + 1], ic = indices[t + 2];

			XMVECTOR a = XMVectorSet(vertices[ia].x, vertices[ia].y, vertices[ia].z, 0.0f);
			XMVECTOR b = XMVectorSet(vertices[ib].x, vertices[ib].y, vertices[ib].z, 0.0f);
			XMVECTOR c = XMVectorSet(vertices[ic].x, vertices[ic].y, vertices[ic].z, 0.0f);

			XMVECTOR faceN = XMVector3Cross(b - a, c - a);
			XMFLOAT3 fn; XMStoreFloat3(&fn, faceN);

			vertices[ia].nx += fn.x; vertices[ia].ny += fn.y; vertices[ia].nz += fn.z;
			vertices[ib].nx += fn.x; vertices[ib].ny += fn.y; vertices[ib].nz += fn.z;
			vertices[ic].nx += fn.x; vertices[ic].ny += fn.y; vertices[ic].nz += fn.z;
		}

		std::map<std::tuple<int, int, int>, XMFLOAT3> welded;

		auto key = [](const Vertex& v) {
			return std::make_tuple(int(std::lround(v.x * 100000.0f)),
				int(std::lround(v.y * 100000.0f)),
				int(std::lround(v.z * 100000.0f))
			); };

		for (auto& v : vertices) {
			auto& n = welded[key(v)];
			n.x += v.nx; n.y += v.ny; n.z += v.nz;
		}

		for(auto& v : vertices){
			auto& n = welded[key(v)];
			v.nx = n.x; v.ny = n.y; v.nz = n.z;
		}

		for (auto& v : vertices) {
			XMVECTOR n = XMVector3Normalize(XMVectorSet(v.nx, v.ny, v.nz, 0.0f));
			XMFLOAT3 nf; XMStoreFloat3(&nf, n);
			v.nx = nf.x; v.ny = nf.y; v.nz = nf.z;
		}

		m_planetMesh = std::make_unique<Mesh>(m_device.Get(), vertices.data(), (UINT)vertices.size(), indices.data(), (UINT)indices.size());
	}

	void Renderer::DrawAtmosphere(XMFLOAT3 camPos ) {

		static XMFLOAT3 planetCenter = { 0.0f, -1000.0f, 0.0f };
		static XMFLOAT3 rayleighCoeff = {5.8f*4.0f, 13.5f*4.0f, 33.1f*4.0f};
		static float innerRadius = 1.0f;
		static float outerRadius = 1.025f;
		static float scaleHeight = 0.003f;
		static float sunIntensity = 20.0f;

		if (ImGui::CollapsingHeader("Atmosphere")) {
			ImGui::DragFloat3("Planet center", &planetCenter.x);
			ImGui::ColorPicker3("Rayleigh", &rayleighCoeff.x);
			ImGui::DragFloat("Inner Radius", &innerRadius, 0.01f);
			ImGui::DragFloat("Outer Radius", &outerRadius, 0.01f);
			ImGui::DragFloat("Scale height", &scaleHeight, 0.001f);
			ImGui::DragFloat("Sun intensity", &sunIntensity, 0.1f);
		}

		XMMATRIX inverseViewProjection = XMMatrixInverse(nullptr, m_viewMatrix * m_projMatrix);
		
		atmosphereConstants ac = {};
		XMStoreFloat4x4(&ac.invViewProj, XMMatrixTranspose( inverseViewProjection ));
		XMStoreFloat3(&ac.camPos, XMVectorSet(camPos.x, camPos.y, camPos.z, 0.0f));
		XMStoreFloat3(&ac.dirToSun, XMVectorSet(-m_lightDir.x, -m_lightDir.y, -m_lightDir.z, 0.0f));
		XMStoreFloat3(&ac.rayleighCoeff, XMVectorSet(rayleighCoeff.x, rayleighCoeff.y, rayleighCoeff.z, 0.0f));
		XMStoreFloat3(&ac.planetCenter, XMVectorSet(planetCenter.x, planetCenter.y, planetCenter.z, 0.0f));
		XMStoreFloat(&ac.innerRadius, XMVectorSet(innerRadius, 0.0f, 0.0f, 0.0f));
		XMStoreFloat(&ac.outerRadius, XMVectorSet(outerRadius, 0.0f, 0.0f, 0.0f));
		XMStoreFloat(&ac.scaleHeight, XMVectorSet(scaleHeight, 0.0f, 0.0f, 0.0f));
		XMStoreFloat(&ac.sunIntensity, XMVectorSet(sunIntensity, 0.0f, 0.0f, 0.0f));


		D3D11_MAPPED_SUBRESOURCE mapped = {};
		m_context->Map(m_atmosphereBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		memcpy(mapped.pData, &ac, sizeof(ac));
		m_context->Unmap(m_atmosphereBuffer.Get(), 0);

		m_context->VSSetConstantBuffers(0, 1, m_atmosphereBuffer.GetAddressOf());
		m_context->PSSetConstantBuffers(0, 1, m_atmosphereBuffer.GetAddressOf());


		m_context->OMSetRenderTargets(1, m_rtv.GetAddressOf(), nullptr);
		m_context->VSSetShader(m_atmoVS.Get(), nullptr, 0);
		m_context->PSSetShader(m_atmoPS.Get(), nullptr, 0);
		m_context->IASetInputLayout(nullptr);
		ID3D11Buffer* nullVB = nullptr; UINT s = 0, o = 0;
		m_context->IASetVertexBuffers(0, 1, &nullVB, &s, &o);
		m_context->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		m_context->PSSetShaderResources(0, 1, m_depthSrv.GetAddressOf());

		float bf[4] = { 0, 0, 0, 0 };
		m_context->OMSetBlendState(m_additiveBlend.Get(), bf, 0xffffffff);

		m_context->Draw(3, 0);

		ID3D11ShaderResourceView* nsrv = nullptr;
		m_context->PSSetShaderResources(0, 1, &nsrv);

		m_context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
	}
}


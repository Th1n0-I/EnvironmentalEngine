#include "pch.h"
#include "Renderer.h"
#include "Mesh.h"
#include "GameObject.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"
#include "FastNoiseLite.h"
#include "MathHelper.h"
#include "Node.h"
#include "PlanetRenderer.h"
#include <stdexcept>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <Lights.h>
#include <memory>
#include <vector>
#include <map>
#include <tuple>
#include <cmath>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

const static float PI = 3.14159265358987;

static constexpr UINT SHADOW_RES = 2048;

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

struct PerPlanetConstants {
	XMFLOAT4X4 transform;
	XMFLOAT4X4 world;
	XMFLOAT4X4 normal;
	XMFLOAT4 cubeColor;
	float specularIntensity;
	float smoothness;
	float percipitationThingy;
	float elevationStrenght;
	float ElevationTemperatureScale;
	float padding[3];
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
	float mieCoeff;
	float mieScaleHeight;
	float mieG;
	float padding0;
};

struct tonemapConstants {
	float exposure;
	XMFLOAT3 padding;
};

struct shadowConstants {
	XMFLOAT4X4 transform;
};

static_assert(sizeof(PerObjectConstants) % 16 == 0, "PerObjectConstants is the wrong size");

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

		D3D11_TEXTURE2D_DESC hdrd = {};
		hdrd.Width = width;
		hdrd.Height = height;
		hdrd.MipLevels = 1;
		hdrd.ArraySize = 1;
		hdrd.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		hdrd.SampleDesc.Count = 1;
		hdrd.Usage = D3D11_USAGE_DEFAULT;
		hdrd.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

		Check(m_device->CreateTexture2D(&hdrd, nullptr, &m_hdrTex));
		Check(m_device->CreateShaderResourceView(m_hdrTex.Get(), nullptr, &m_hdrSrv));
		Check(m_device->CreateRenderTargetView(m_hdrTex.Get(), nullptr, &m_hdrRtv));

		D3D11_TEXTURE2D_DESC dd = {};
		dd.Width = width;
		dd.Height = height;
		dd.MipLevels = 1;
		dd.ArraySize = 1;
		dd.Format = DXGI_FORMAT_R32_TYPELESS;
		dd.SampleDesc.Count = 1;
		dd.Usage = D3D11_USAGE_DEFAULT;
		dd.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

		D3D11_DEPTH_STENCIL_VIEW_DESC dsv = {};
		dsv.Format = DXGI_FORMAT_D32_FLOAT;
		dsv.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		dsv.Texture2D.MipSlice = 0;
		dsv.Flags = 0;

		D3D11_SHADER_RESOURCE_VIEW_DESC srv = {};
		srv.Format = DXGI_FORMAT_R32_FLOAT;
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
		m_viewport = vp;
		m_context->RSSetViewports(1, &m_viewport);
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
		m_viewport = vp;
		m_context->RSSetViewports(1, &m_viewport);

		vp.Width = SHADOW_RES;
		vp.Height = SHADOW_RES;
		vp.MaxDepth = 1.0f;
		m_shadowViewport = vp;

		D3D11_SAMPLER_DESC sd = {};
		sd.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		sd.AddressU = sd.AddressW = sd.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
		sd.BorderColor[0] = sd.BorderColor[1] = sd.BorderColor[2] = sd.BorderColor[3] = 0.0f;
		sd.ComparisonFunc = D3D11_COMPARISON_GREATER_EQUAL;
		sd.MaxLOD = D3D11_FLOAT32_MAX;
		Check(m_device->CreateSamplerState(&sd, &m_shadowSampler));

		D3D11_TEXTURE2D_DESC hdrd = {};
		hdrd.Width = width;
		hdrd.Height = height;
		hdrd.MipLevels = 1;
		hdrd.ArraySize = 1;
		hdrd.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		hdrd.SampleDesc.Count = 1;
		hdrd.Usage = D3D11_USAGE_DEFAULT;
		hdrd.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

		Check(m_device->CreateTexture2D(&hdrd, nullptr, &m_hdrTex));
		Check(m_device->CreateShaderResourceView(m_hdrTex.Get(), nullptr, &m_hdrSrv));
		Check(m_device->CreateRenderTargetView(m_hdrTex.Get(), nullptr, &m_hdrRtv));

		D3D11_TEXTURE2D_DESC dd = {};
		dd.Width = width;
		dd.Height = height;
		dd.MipLevels = 1;
		dd.ArraySize = 1;
		dd.Format = DXGI_FORMAT_R32_TYPELESS;
		dd.SampleDesc.Count = 1;
		dd.Usage = D3D11_USAGE_DEFAULT;
		dd.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

		D3D11_DEPTH_STENCIL_VIEW_DESC dsv = {};
		dsv.Format = DXGI_FORMAT_D32_FLOAT;
		dsv.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		dsv.Texture2D.MipSlice = 0;
		dsv.Flags = 0;

		D3D11_DEPTH_STENCIL_DESC dsd = {};
		dsd.DepthEnable = TRUE;
		dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		dsd.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;

		D3D11_SHADER_RESOURCE_VIEW_DESC srv = {};
		srv.Format = DXGI_FORMAT_R32_FLOAT;
		srv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srv.Texture2D.MostDetailedMip = 0;
		srv.Texture2D.MipLevels = 1;

		Check(m_device->CreateTexture2D(&dd, nullptr, &m_depthTex));
		Check(m_device->CreateDepthStencilView(m_depthTex.Get(), &dsv, &m_depthView));
		Check(m_device->CreateShaderResourceView(m_depthTex.Get(), &srv, &m_depthSrv));
		Check(m_device->CreateDepthStencilState(&dsd, &m_depthState));

		// Create shadow texture
		D3D11_TEXTURE2D_DESC std = {};
		std.Width = SHADOW_RES;
		std.Height = SHADOW_RES;
		std.MipLevels = 1;
		std.ArraySize = 1;
		std.Format = DXGI_FORMAT_R32_TYPELESS;
		std.SampleDesc.Count = 1;
		std.Usage = D3D11_USAGE_DEFAULT;
		std.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

		// Since the shadow texture does the same thing as the depth texture, we can reuse the other thingies. Hopefully. I'm not sure.
		Check(m_device->CreateTexture2D(&std, nullptr, &m_shadowTex));
		Check(m_device->CreateDepthStencilView(m_shadowTex.Get(), &dsv, &m_shadowView));
		Check(m_device->CreateShaderResourceView(m_shadowTex.Get(), &srv, &m_shadowSrv));
		Check(m_device->CreateDepthStencilState(&dsd, &m_shadowState));
        
		CreateCube();
	

		m_planet = std::make_unique<PlanetRenderer>(
			m_device.Get(), XMFLOAT3{ 0.0f, -100000.0f, 0.0f }, 100000.0f
			);

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

		D3D11_BUFFER_DESC tbd = {};
		tbd.ByteWidth = sizeof(tonemapConstants);
		tbd.Usage = D3D11_USAGE_DYNAMIC;
		tbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		tbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		Check(m_device->CreateBuffer(&tbd, nullptr, &m_tonemapBuffer));
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
		m_context->OMSetRenderTargets(1, m_hdrRtv.GetAddressOf(), m_depthView.Get());
		m_context->ClearRenderTargetView(m_hdrRtv.Get(), clear);
		m_context->ClearDepthStencilView(m_depthView.Get(), D3D11_CLEAR_DEPTH, 0.0f, 0);
		m_context->OMSetDepthStencilState(m_depthState.Get(), 0);

		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("Environmental Engine");
	

		

		m_viewMatrix = view;
		m_projMatrix = XMMatrixPerspectiveFovLH(
			XMConvertToRadians(m_fov),
			aspect_ratio,
			500000.0f,
			1.0f);

		
		
		

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

		m_planet->Update(m_device.Get(), camPos);
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

	void Renderer::DrawNode(ID3D11DeviceContext* ctx, const node& n) {
		if (isLeaf(n)) {
			if (n.mesh) { n.mesh->Bind(ctx); ctx->DrawIndexed(n.mesh->IndexCount(), 0, 0); }
		}
		else {
			for (auto& c : n.children) DrawNode(ctx, *c);
		}
	}

	void Renderer::DrawPlanet() {

		static float pT = 1.07f;
		static float elevationStrength = 1.0f;
		static float elevationTemperatureScale = 6.5f;

		ImGui::DragFloat("Percipitation thingy", &pT, 0.01f);
		ImGui::DragFloat("Elevation strength ", &elevationStrength, 0.01f);
		ImGui::DragFloat("Elevation temperature scale ", &elevationTemperatureScale, 0.01f);

		XMMATRIX world =
			XMMatrixTranslation(m_planet->center.x, m_planet->center.y, m_planet->center.z);

		XMMATRIX final = world * m_viewMatrix * m_projMatrix;

		XMMATRIX normal = XMMatrixInverse(nullptr, world);

		PerPlanetConstants constants = {};
		XMStoreFloat4x4(&constants.transform, XMMatrixTranspose(final));
		XMStoreFloat4x4(&constants.world, XMMatrixTranspose(world));
		XMStoreFloat4x4(&constants.normal, normal);

		XMStoreFloat4(&constants.cubeColor, XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f));
		XMStoreFloat(&constants.specularIntensity, XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f));
		XMStoreFloat(&constants.smoothness, XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f));
		XMStoreFloat(&constants.percipitationThingy, XMVectorSet(pT, 0.0f, 0.0f, 0.0f));
		XMStoreFloat(&constants.elevationStrenght, XMVectorSet(elevationStrength, 0.0f, 0.0f, 0.0f));
		XMStoreFloat(&constants.ElevationTemperatureScale, XMVectorSet(elevationTemperatureScale, 0.0f, 0.0f, 0.0f));

		D3D11_MAPPED_SUBRESOURCE mapped = {};
		m_context->Map(m_perPlanetBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		memcpy(mapped.pData, &constants, sizeof(constants));
		m_context->Unmap(m_perPlanetBuffer.Get(), 0);

		//m_context->RSSetState(m_wireframe.Get());
		if (!m_shadowPass) m_context->VSSetShader(m_terrainVS.Get(), nullptr, 0);
		m_context->VSSetConstantBuffers(1, 1, m_perPlanetBuffer.GetAddressOf());
		if (!m_shadowPass) m_context->PSSetShader(m_terrainPS.Get(), nullptr, 0);
		m_context->PSSetConstantBuffers(1, 1, m_perPlanetBuffer.GetAddressOf());
		m_context->IASetInputLayout(m_terrainInputLayout.Get());

		m_context->PSSetShaderResources(0, 1, m_biomeSrv.GetAddressOf());  
		m_context->PSSetSamplers(0, 1, m_biomeSampler.GetAddressOf());   

		m_context->PSSetShaderResources(1, 1, m_biomeIdSrv.GetAddressOf());
		m_context->PSSetSamplers(1, 1, m_biomeIdSampler.GetAddressOf());

		for (auto& n : m_planet->roots) {
			DrawNode(m_context.Get(), *n);
		}

		//m_context->RSSetState(nullptr);
	}

	XMMATRIX Renderer::BuildLightMatrix(XMFLOAT3 focus, float extent, float depth) 
	{
		XMVECTOR dir = XMVector3Normalize(XMLoadFloat3(&m_lightDir));
		XMVECTOR target = XMLoadFloat3(&focus);
		XMVECTOR eye = target - dir * depth * 0.5f;

		XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		if(fabsf(XMVectorGetX(XMVector3Dot(dir, up))) > 0.99f) {
			up = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
		}

		XMMATRIX lightView = XMMatrixLookToLH(eye, dir, up);
		XMMATRIX lightProj = XMMatrixOrthographicLH(extent, extent, depth, 0.1f);
		return lightView * lightProj;
	}

	void Renderer::BeginShadowPass(XMFLOAT3 focus) {
		m_shadowPass = true;

		XMVECTOR c = XMLoadFloat3(&m_planet->center);
		XMVECTOR p = XMLoadFloat3(&focus);
		XMVECTOR surface = c + XMVector3Normalize(p - c) * m_planet->innerRadius;
		XMStoreFloat3(&focus, surface);


		ID3D11ShaderResourceView* nullsrv = nullptr;
		m_context->PSSetShaderResources(2, 1, &nullsrv);

		XMMATRIX lightMatrix = BuildLightMatrix(focus, 20000.0f, 20000.0f + 2 * m_planet->radius * 0.05f);

		shadowConstants co = {};
		XMStoreFloat4x4(&co.transform, XMMatrixTranspose(lightMatrix));

		D3D11_MAPPED_SUBRESOURCE mapped = {};
		m_context->Map(m_shadowBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		memcpy(mapped.pData, &co, sizeof(co));
		m_context->Unmap(m_shadowBuffer.Get(), 0);

		m_savedProjMatrix = m_projMatrix;
		m_savedViewMatrix = m_viewMatrix;
		m_projMatrix = XMMatrixIdentity();
		m_viewMatrix = lightMatrix;

		m_context->RSSetViewports(1, &m_shadowViewport);
		m_context->OMSetRenderTargets(0, nullptr, m_shadowView.Get());
		m_context->ClearDepthStencilView(m_shadowView.Get(), D3D11_CLEAR_DEPTH, 0.0f, 0);
		m_context->OMSetDepthStencilState(m_shadowState.Get(), 0);

		m_context->VSSetShader(m_shadowVS.Get(), nullptr, 0);
		m_context->PSSetShader(nullptr, nullptr, 0);
		m_context->IASetInputLayout(m_inputLayout.Get());

	}

	void Renderer::EndShadowPass() {
		m_shadowPass = false;
		m_viewMatrix = m_savedViewMatrix;
		m_projMatrix = m_savedProjMatrix;

		m_context->RSSetViewports(1, &m_viewport);
		m_context->OMSetRenderTargets(1, m_hdrRtv.GetAddressOf(), m_depthView.Get());
		m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
		m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);

		m_context->PSSetConstantBuffers(2, 1, m_shadowBuffer.GetAddressOf());
		m_context->PSSetShaderResources(2, 1, m_shadowSrv.GetAddressOf());
		m_context->PSSetSamplers(2, 1, m_shadowSampler.GetAddressOf());	
	}

	void Renderer::EndFrame() 
    {
		static float exposure = 1.0;


		if (ImGui::CollapsingHeader("Tonemap")) {
			ImGui::DragFloat("Exposure", &exposure, 0.01f);
		}


		tonemapConstants consts = {};
		XMStoreFloat(&consts.exposure, XMVectorSet(exposure, 0.0f, 0.0f, 0.0f));

		
		D3D11_MAPPED_SUBRESOURCE mapped = {};
		m_context->Map(m_tonemapBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		memcpy(mapped.pData, &consts, sizeof(consts));
		m_context->Unmap(m_tonemapBuffer.Get(), 0);

		m_context->VSSetConstantBuffers(0, 1, m_tonemapBuffer.GetAddressOf());
		m_context->PSSetConstantBuffers(0, 1, m_tonemapBuffer.GetAddressOf());

		m_context->OMSetRenderTargets(1, m_rtv.GetAddressOf(), nullptr);

		m_context->PSSetShaderResources(0, 1, m_hdrSrv.GetAddressOf());

		m_context->VSSetShader(m_tonemapVS.Get(), nullptr, 0);
		m_context->PSSetShader(m_tonemapPS.Get(), nullptr, 0);
		m_context->IASetInputLayout(nullptr);
		m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_context->Draw(3, 0);
		ID3D11ShaderResourceView* nullsrv = nullptr;
		m_context->PSSetShaderResources(0, 1, &nullsrv);


		ImGui::End();
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		m_swapChain->Present(1, 0);
	}

	void Renderer::CreateCube()
    {
		Vertex vertices[] =
		{//     x      y      z         nx     ny     nz  elevation
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

		m_cubeMesh = std::make_unique<Mesh>(m_device.Get(), vertices, sizeof(vertices) / sizeof(vertices[0]), sizeof(Vertex), indices, sizeof(indices) / sizeof(indices[0]));

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
				sVertices.push_back({ x, y, z, x / r, y / r, z / r});
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

		m_sphereMesh = std::make_unique<Mesh>(m_device.Get(), sVertices.data(), (UINT)sVertices.size(), sizeof(Vertex), sIndices.data(), (UINT)sIndices.size());

		D3D11_BUFFER_DESC cbd = {};
		cbd.ByteWidth = sizeof(PerFrameConstants);
		cbd.Usage = D3D11_USAGE_DYNAMIC;
		cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		Check(m_device->CreateBuffer(&cbd, nullptr, &m_perFrameBuffer));

		cbd.ByteWidth = sizeof(PerObjectConstants);
		
		Check(m_device->CreateBuffer(&cbd, nullptr, &m_perObjectBuffer));

		cbd.ByteWidth = sizeof(PerPlanetConstants);

		Check(m_device->CreateBuffer(&cbd, nullptr, &m_perPlanetBuffer));

		cbd.ByteWidth = sizeof(atmosphereConstants);

		Check(m_device->CreateBuffer(&cbd, nullptr, &m_atmosphereBuffer));

		cbd.ByteWidth = sizeof(shadowConstants);

		Check(m_device->CreateBuffer(&cbd, nullptr, &m_shadowBuffer));

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

		std::wstring tonemapPath = ExeDir() + L"toneMap.hlsl";
		auto tmvs = LoadShaderByteCode(tonemapPath.c_str(), "VSMain", "vs_5_0");
		auto tmps = LoadShaderByteCode(tonemapPath.c_str(), "PSMain", "ps_5_0");

		Check(m_device->CreateVertexShader(
			tmvs->GetBufferPointer(), tmvs->GetBufferSize(),
			nullptr, &m_tonemapVS
		));

		Check(m_device->CreatePixelShader(
			tmps->GetBufferPointer(), tmps->GetBufferSize(),
			nullptr, &m_tonemapPS
		));

		std::wstring terrainPath = ExeDir() + L"Terrain.hlsl";
		auto tvs = LoadShaderByteCode(terrainPath.c_str(), "VSMain", "vs_5_0");
		auto tps = LoadShaderByteCode(terrainPath.c_str(), "PSMain", "ps_5_0");

		Check(m_device->CreateVertexShader(
			tvs->GetBufferPointer(), tvs->GetBufferSize(),
			nullptr, &m_terrainVS
		));

		Check(m_device->CreatePixelShader(
			tps->GetBufferPointer(), tps->GetBufferSize(),
			nullptr, &m_terrainPS
		));

		std::wstring shadowPath = ExeDir() + L"Shadow.hlsl";
		auto svs = LoadShaderByteCode(shadowPath.c_str(), "VSMain", "vs_5_0");

		Check(m_device->CreateVertexShader(
			svs->GetBufferPointer(), svs->GetBufferSize(),
			nullptr, &m_shadowVS
		));

		D3D11_INPUT_ELEMENT_DESC terrainLayout[] = {
			{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"ELEVATION", 0, DXGI_FORMAT_R32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TEMPERATURE", 0, DXGI_FORMAT_R32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"PERCIPITATION", 0, DXGI_FORMAT_R32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0},
		};

		Check(m_device->CreateInputLayout(
			terrainLayout, 5,
			tvs->GetBufferPointer(), tvs->GetBufferSize(),
			&m_terrainInputLayout
		));

		std::wstring texturePath = ExeDir() + L"BiomeMap.png";
		FILE* textureFile;
		if(_wfopen_s(&textureFile, texturePath.c_str(), L"rb") != 0 || !textureFile)
		{
			throw std::runtime_error("biomeMap.png not found");
		}
		int w, h, n;
		unsigned char* px = stbi_load_from_file(textureFile, &w, &h, &n, 4);
		fclose(textureFile);
		if (!px) throw std::runtime_error("biomeMap.png failed to decode");

		D3D11_TEXTURE2D_DESC td = {};
		td.Width = w; td.Height = h;
		td.MipLevels = 1; td.ArraySize = 1;
		td.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		td.SampleDesc.Count = 1;
		td.Usage = D3D11_USAGE_DEFAULT;
		td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		D3D11_SUBRESOURCE_DATA sd = {};
		sd.pSysMem = px;
		sd.SysMemPitch = w * 4;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> biomeTex = {};
		Check(m_device->CreateTexture2D(&td, &sd, &biomeTex));
		Check(m_device->CreateShaderResourceView(biomeTex.Get(), nullptr, &m_biomeSrv));
		stbi_image_free(px);

		D3D11_SAMPLER_DESC smp = {};
		smp.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		smp.AddressU = smp.AddressV = smp.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		smp.MaxLOD = D3D11_FLOAT32_MAX;
		Check(m_device->CreateSamplerState(&smp, &m_biomeSampler));

		td = {};
		sd = {};
		smp = {};

		texturePath = ExeDir() + L"BiomeIDMap.png";
		if (_wfopen_s(&textureFile, texturePath.c_str(), L"rb") != 0 || !textureFile)
		{
			throw std::runtime_error("biomeIDMap.png not found");
		}
		w = 0, h = 0, n = 0;
		px = stbi_load_from_file(textureFile, &w, &h, &n, 1);
		fclose(textureFile);
		if (!px) throw std::runtime_error("biomeIDMap.png failed to decode");

		td.Width = w; td.Height = h;
		td.MipLevels = 1; td.ArraySize = 1;
		td.Format = DXGI_FORMAT_R8_UINT;
		td.SampleDesc.Count = 1;
		td.Usage = D3D11_USAGE_DEFAULT;
		td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		sd.pSysMem = px;
		sd.SysMemPitch = w;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> biomeIdTex = {};
		Check(m_device->CreateTexture2D(&td, &sd, &biomeIdTex));
		Check(m_device->CreateShaderResourceView(biomeIdTex.Get(), nullptr, &m_biomeIdSrv));
		stbi_image_free(px);

		smp.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		smp.AddressU = smp.AddressV = smp.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		smp.MaxLOD = D3D11_FLOAT32_MAX;
		Check(m_device->CreateSamplerState(&smp, &m_biomeIdSampler));

		D3D11_RASTERIZER_DESC rd = {};
		rd.FillMode = D3D11_FILL_WIREFRAME;
		rd.CullMode = D3D11_CULL_NONE;
		m_device->CreateRasterizerState(&rd, &m_wireframe);
    }


	void Renderer::DrawAtmosphere(XMFLOAT3 camPos ) {
		static bool drawAtmosphere = true;

		if (ImGui::CollapsingHeader("Atmosphere")) {
			if (ImGui::Button("Toggle atmosphere")) drawAtmosphere = !drawAtmosphere;
			ImGui::DragFloat3("Planet center", &m_planet->center.x);
			ImGui::ColorPicker3("Rayleigh coefficient", &m_planet->rayleighCoeff.x);
			ImGui::DragFloat("Inner Radius", &m_planet->innerRadius, 0.01f);
			ImGui::DragFloat("Outer Radius", &m_planet->outerRadius, 0.01f);
			ImGui::DragFloat("Scale height", &m_planet->scaleHeight, 0.001f);
			ImGui::DragFloat("Sun intensity", &m_planet->sunIntensity, 0.1f);
			ImGui::DragFloat("Mie coefficient", &m_planet->mieCoeff, 0.001f);
			ImGui::DragFloat("Mie scale height", &m_planet->mieScaleHeight, 0.01f);
			ImGui::DragFloat("Mie scatter G", &m_planet->mieG, 0.01f);
		}

		if (!drawAtmosphere) return;

		XMMATRIX inverseViewProjection = XMMatrixInverse(nullptr, m_viewMatrix * m_projMatrix);
		
		atmosphereConstants ac = {};
		XMStoreFloat4x4(&ac.invViewProj, XMMatrixTranspose( inverseViewProjection ));
		XMStoreFloat3(&ac.camPos, XMVectorSet(camPos.x, camPos.y, camPos.z, 0.0f));
		XMStoreFloat3(&ac.dirToSun, XMVectorSet(-m_lightDir.x, -m_lightDir.y, -m_lightDir.z, 0.0f));
		XMStoreFloat3(&ac.rayleighCoeff, XMVectorSet(m_planet->rayleighCoeff.x, m_planet->rayleighCoeff.y, m_planet->rayleighCoeff.z, 0.0f));
		XMStoreFloat3(&ac.planetCenter, XMVectorSet(m_planet->center.x, m_planet->center.y, m_planet->center.z, 0.0f));
		XMStoreFloat(&ac.innerRadius, XMVectorSet(m_planet->innerRadius, 0.0f, 0.0f, 0.0f));
		XMStoreFloat(&ac.outerRadius, XMVectorSet(m_planet->outerRadius, 0.0f, 0.0f, 0.0f));
		XMStoreFloat(&ac.scaleHeight, XMVectorSet(m_planet->scaleHeight, 0.0f, 0.0f, 0.0f));
		XMStoreFloat(&ac.sunIntensity, XMVectorSet(m_planet->sunIntensity, 0.0f, 0.0f, 0.0f));
		XMStoreFloat(&ac.mieCoeff, XMVectorSet(m_planet->mieCoeff, 0.0f, 0.0f, 0.0f));
		XMStoreFloat(&ac.mieScaleHeight, XMVectorSet(m_planet->mieScaleHeight, 0.0f, 0.0f, 0.0f));
		XMStoreFloat(&ac.mieG, XMVectorSet(m_planet->mieG, 0.0f, 0.0f, 0.0f));


		D3D11_MAPPED_SUBRESOURCE mapped = {};
		m_context->Map(m_atmosphereBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		memcpy(mapped.pData, &ac, sizeof(ac));
		m_context->Unmap(m_atmosphereBuffer.Get(), 0);

		m_context->VSSetConstantBuffers(0, 1, m_atmosphereBuffer.GetAddressOf());
		m_context->PSSetConstantBuffers(0, 1, m_atmosphereBuffer.GetAddressOf());
		m_context->PSSetConstantBuffers(2, 1, m_shadowBuffer.GetAddressOf());


		m_context->OMSetRenderTargets(1, m_hdrRtv.GetAddressOf(), nullptr);
		m_context->VSSetShader(m_atmoVS.Get(), nullptr, 0);
		m_context->PSSetShader(m_atmoPS.Get(), nullptr, 0);
		m_context->IASetInputLayout(nullptr);
		ID3D11Buffer* nullVB = nullptr; UINT s = 0, o = 0;
		m_context->IASetVertexBuffers(0, 1, &nullVB, &s, &o);
		m_context->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		m_context->PSSetShaderResources(0, 1, m_depthSrv.GetAddressOf());
		m_context->PSSetShaderResources(2, 1, m_shadowSrv.GetAddressOf());
		m_context->PSSetSamplers(2, 1, m_shadowSampler.GetAddressOf());

		float bf[4] = { 0, 0, 0, 0 };
		m_context->OMSetBlendState(m_additiveBlend.Get(), bf, 0xffffffff);

		m_context->Draw(3, 0);

		ID3D11ShaderResourceView* nsrv = nullptr;
		m_context->PSSetShaderResources(0, 1, &nsrv);

		m_context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
	}
}


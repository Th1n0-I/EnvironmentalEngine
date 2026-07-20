
#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <Camera.h>
#include <DirectXMath.h>
#include <Lights.h>
#include "Mesh.h"
#include "MeshRenderer.h"
#include <memory>

namespace EnvironmentalEngine {
	class Renderer {
	public:
		Renderer(HWND hwnd, int width, int height);
		~Renderer();

		Renderer(const Renderer&) = delete;
		Renderer& operator = (const Renderer&) = delete;

		void BeginFrame(int width, int height, float deltaTime, const DirectX::XMMATRIX& view, DirectX::XMFLOAT3 camPos, DirectionalLight& dl, AmbientLight& al, PointLight& pl);
		void EndFrame();   

		void Resize(int width, int height);

		void Draw(const MeshRenderer& mr);

		Mesh* CubeMesh() const { return m_cubeMesh.get(); }
	
	private:
		Microsoft::WRL::ComPtr<ID3D11Device> m_device;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
		Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_rtv;
	    
	    void CreateCube();
	    
		std::unique_ptr<Mesh> m_cubeMesh;
	    
	    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
	    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;
	    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;

		Microsoft::WRL::ComPtr<ID3D11Buffer> m_perFrameBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_perObjectBuffer;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> m_depthTex;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_depthView;

		DirectX::XMMATRIX m_viewMatrix;
		DirectX::XMMATRIX m_projMatrix;

		float m_fov = 60.0f;
	};
}
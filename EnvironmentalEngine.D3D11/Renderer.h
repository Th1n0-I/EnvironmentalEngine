
#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <Camera.h>
#include <DirectXMath.h>
#include <Lights.h>
#include "Mesh.h"
#include "Node.h"
#include "MeshRenderer.h"
#include "GameObject.h"
#include "PlanetRenderer.h"
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

		void Draw(const MeshRenderer& mr, const Transform& tr);
		void DrawPlanet();
		void DrawAtmosphere(DirectX::XMFLOAT3 camPos);
		void DrawNode(ID3D11DeviceContext* ctx, const node& n);

		Mesh* CubeMesh() const { return m_cubeMesh.get(); }
		Mesh* SphereMesh() const { return m_sphereMesh.get(); }
		Mesh* PlanetMesh() const { return m_planetMesh.get(); }

	private:
		Microsoft::WRL::ComPtr<ID3D11Device> m_device;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
		Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_rtv;
		Microsoft::WRL::ComPtr<ID3D11RasterizerState>  m_wireframe;
	    
	    void CreateCube();
		void CreatePlanet(float radius, UINT res);
	    
		std::unique_ptr<Mesh> m_cubeMesh;
		std::unique_ptr<Mesh> m_sphereMesh;
		std::unique_ptr<Mesh> m_planetMesh;
		std::vector<std::unique_ptr<Mesh>> m_chunks;
	    
	    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
	    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;
	    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;

		Microsoft::WRL::ComPtr<ID3D11Buffer> m_perFrameBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_perObjectBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_atmosphereBuffer;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> m_depthTex;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_depthView;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_depthSrv;

		Microsoft::WRL::ComPtr<ID3D11VertexShader> m_atmoVS;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_atmoPS;

		Microsoft::WRL::ComPtr<ID3D11BlendState> m_additiveBlend;

		DirectX::XMMATRIX m_viewMatrix;
		DirectX::XMMATRIX m_projMatrix;

		DirectX::XMFLOAT3 m_lightDir;

		std::unique_ptr<PlanetRenderer> m_planet;

		float m_fov = 60.0f;
	};

	
}
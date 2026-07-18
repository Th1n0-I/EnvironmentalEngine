
#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <Camera.h>

namespace EnvironmentalEngine {
	class Renderer {
	public:
		Renderer(HWND hwnd, int width, int height);
		~Renderer();

		Renderer(const Renderer&) = delete;
		Renderer& operator = (const Renderer&) = delete;

		void BeginFrame(int width, int height, float deltaTime);
		void EndFrame();   

		void Resize(int width, int height);
	
	private:
		Microsoft::WRL::ComPtr<ID3D11Device> m_device;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
		Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_rtv;
	    
	    void CreateCube();
	    
	    Microsoft::WRL::ComPtr<ID3D11Buffer> m_vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_indexBuffer;
	    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
	    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;
	    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;

		Microsoft::WRL::ComPtr<ID3D11Buffer> m_constantBuffer;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> m_depthTex;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_depthView;

		float m_spinSpeed = 1.0f;
		float m_fov = 60.0f;

		UINT m_indexCount = 0;

		Camera m_camera;
	};
}
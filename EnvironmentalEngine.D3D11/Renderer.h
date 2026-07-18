
#pragma once
#include <d3d11.h>
#include <wrl/client.h>

namespace EnvironmentalEngine {
	class Renderer {
	public:
		Renderer(HWND hwnd, int width, int height);

		Renderer(const Renderer&) = delete;
		Renderer& operator = (const Renderer&) = delete;

		void BeginFrame();
		void EndFrame();   
	
	private:
		Microsoft::WRL::ComPtr<ID3D11Device> m_device;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
		Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_rtv;
	    
	    void CreateTriangle();
	    
	    Microsoft::WRL::ComPtr<ID3D11Buffer> m_vertexBuffer;
	    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
	    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;
	    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;

		Microsoft::WRL::ComPtr<ID3D11Buffer> m_constantBuffer;
	};
}
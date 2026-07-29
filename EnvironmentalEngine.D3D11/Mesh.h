#pragma once
#include <d3d11.h>
#include <wrl/client.h>


namespace EnvironmentalEngine {

	struct Vertex
	{
		float x, y, z, nx, ny, nz;
	};

	struct TerrainVertex {
		float x, y, z, nx, ny, nz;
		float elevation, temperature, percipitation;
	};
 

	class Mesh {
	public:
		Mesh(ID3D11Device* device,
			const void* vertexData, UINT vertexCount, UINT vertexStride,
			const UINT* indices, UINT indexCount);

		void Bind(ID3D11DeviceContext* context) const;
		UINT IndexCount() const { return m_indexCount; }
	private:
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_indexBuffer;
		UINT m_indexCount = 0;
		UINT m_vertexStride = 0;
	};
}
#include "pch.h"
#include "Mesh.h"

#include <d3d11.h>
#include <wrl/client.h>
#include <stdexcept>

inline void Check(HRESULT hr)
{
	if (FAILED(hr))
	{
		throw std::runtime_error("D3D11 Call failed!");
	}
}

namespace EnvironmentalEngine {
	Mesh::Mesh(ID3D11Device* device, const Vertex* vertices, UINT vertexCount, const unsigned int* indices, UINT indexCount) {

		D3D11_BUFFER_DESC ibd = {};
		ibd.ByteWidth = indexCount * sizeof(UINT);
		ibd.Usage = D3D11_USAGE_DEFAULT;
		ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;

		D3D11_SUBRESOURCE_DATA iinit = {};
		iinit.pSysMem = indices;

		Check(device->CreateBuffer(&ibd, &iinit, &m_indexBuffer));

		D3D11_BUFFER_DESC bd = {};
		bd.ByteWidth = vertexCount * sizeof(Vertex);
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

		D3D11_SUBRESOURCE_DATA init = {};
		init.pSysMem = vertices;

		Check(device->CreateBuffer(&bd, &init, &m_vertexBuffer));

		m_indexCount = indexCount;
		m_vertexStride = sizeof(Vertex);
	}

	void Mesh::Bind(ID3D11DeviceContext* context) const {
		
		UINT offset = 0;

		context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &m_vertexStride, &offset);
		context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}
}
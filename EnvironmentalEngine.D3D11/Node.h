#pragma once
#include <DirectXMath.h>
#include <memory>
#include <algorithm>
#include <vector>
#include "Mesh.h"

using namespace DirectX;

namespace EnvironmentalEngine {
	struct chunkData {
		std::vector<Vertex> vertices;
		std::vector<UINT> indices;
	};

	struct node {
		UINT face;
		XMFLOAT2 uvMin, uvMax;
		UINT level;
		XMFLOAT3 center, AABBMin, AABBMax;
		chunkData chunkData;
		std::unique_ptr<node> children[4];
		std::unique_ptr<Mesh> mesh;

		XMFLOAT2 quadMin(UINT i) {
			XMFLOAT2 mid = XMFLOAT2{ (uvMin.x + uvMax.x) * 0.5f, (uvMin.y + uvMax.y) * 0.5f };

			switch (i) {
			case 0:
				return uvMin;
				break;
			case 1:
				return XMFLOAT2{ mid.x, uvMin.y };
				break;
			case 2:
				return XMFLOAT2{ uvMin.x, mid.y };
				break;
			case 3:
				return mid;
			}
		}

		XMFLOAT2 quadMax(UINT i) {
			XMFLOAT2 mid = XMFLOAT2{ (uvMin.x + uvMax.x) * 0.5f, (uvMin.y + uvMax.y) * 0.5f };

			switch (i) {
			case 0:
				return mid;
				break;
			case 1:
				return XMFLOAT2{ uvMax.x, mid.y };
				break;
			case 2:
				return XMFLOAT2{ mid.x, uvMax.y };
				break;
			case 3:
				return uvMax;
			}
		}
	};

	

	chunkData GenerateChunk(UINT face, XMFLOAT2 uvMin, XMFLOAT2 uvMax);

	XMVECTOR CubePos(UINT face, DirectX::XMFLOAT2 uv);

	inline node MakeNode(UINT face, XMFLOAT2 uvMin, XMFLOAT2 uvMax, UINT level) {
		node n = { face, uvMin, uvMax, level };
		XMFLOAT2 uvCenter = XMFLOAT2{ (uvMin.x + uvMax.x) * 0.5f, (uvMin.y + uvMax.y) * 0.5f };
		XMStoreFloat3(&n.center, XMVector3Normalize(CubePos(face, uvCenter)) * 1000.0f);
		n.chunkData = GenerateChunk(face, uvMin, uvMax);
		n.AABBMin = { n.chunkData.vertices[0].x, n.chunkData.vertices[0].y, n.chunkData.vertices[0].z };
		n.AABBMax = n.AABBMin;
		for (auto& v : n.chunkData.vertices) {
			n.AABBMin.x = (std::min)(v.x, n.AABBMin.x); n.AABBMin.y = (std::min)(v.y, n.AABBMin.y); n.AABBMin.z = (std::min)(v.z, n.AABBMin.z);
			n.AABBMax.x = (std::max)(v.x, n.AABBMax.x); n.AABBMax.y = (std::max)(v.y, n.AABBMax.y); n.AABBMax.z = (std::max)(v.z, n.AABBMax.z);
		}

		return n;
	}

	inline void UploadNode(ID3D11Device* device, node& n) {
		n.mesh = std::make_unique<Mesh>(device, n.chunkData.vertices.data(), (UINT)n.chunkData.vertices.size(),
			n.chunkData.indices.data(), (UINT)n.chunkData.indices.size());

		n.chunkData = {};
	}

	inline bool isLeaf(const node& n) {
		return n.children[0] == nullptr;
	}

	void UpdateLOD(ID3D11Device* device, node& n, XMFLOAT3 camPos);
}


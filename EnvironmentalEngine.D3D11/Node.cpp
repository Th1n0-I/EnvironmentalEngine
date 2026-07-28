#include "pch.h"
#include "Node.h"
#include "FastNoiseLite.h"
#include "MathHelper.h"
#include <chrono>


using namespace DirectX;

namespace EnvironmentalEngine {
	XMVECTOR CubePos(UINT face, DirectX::XMFLOAT2 uv) {
		static XMFLOAT3 localUp[6] = {
			{  0.0f,  1.0f,  0.0f },
			{  0.0f, -1.0f,  0.0f },
			{  1.0f,  0.0f,  0.0f },
			{ -1.0f,  0.0f,  0.0f },
			{  0.0f,  0.0f,  1.0f },
			{  0.0f,  0.0f, -1.0f } };
		static XMFLOAT3 axisA[6] = {
			{  1.0f,  0.0f,  0.0f },
			{ -1.0f,  0.0f,  0.0f },
			{  0.0f,  0.0f,  1.0f },
			{  0.0f,  0.0f, -1.0f },
			{  0.0f,  1.0f,  0.0f },
			{  0.0f, -1.0f,  0.0f } };
		static XMFLOAT3 axisB[6] = {
			{  0.0f,  0.0f, -1.0f },
			{  0.0f,  0.0f, -1.0f },
			{  0.0f, -1.0f,  0.0f },
			{  0.0f, -1.0f,  0.0f },
			{ -1.0f,  0.0f,  0.0f },
			{ -1.0f,  0.0f,  0.0f } };

		XMVECTOR uvV = XMVectorSet(uv.x, uv.y, 0.0f, 0.0f);
		XMVECTOR cubePos = XMVectorSet(localUp[face].x, localUp[face].y, localUp[face].z, 0.0f) +
			(uv.x - 0.5f) * 2.0f * XMVectorSet(axisA[face].x, axisA[face].y, axisA[face].z, 0.0f) +
			(uv.y - 0.5f) * 2.0f * XMVectorSet(axisB[face].x, axisB[face].y, axisB[face].z, 0.0f);

		return cubePos;
	}

	bool isSkirt(int res, int v) {
		int R = res + 2;
		int X = v / R;
		int Y = v % R;
		return (X == 0 || X == R - 1 || Y == 0 || Y == R - 1);
	}

	ChunkData GenerateChunk(UINT face, XMFLOAT2 uvMin, XMFLOAT2 uvMax, float radius) {

		int res = 16;

		FastNoiseLite mtnN;
		mtnN.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
		mtnN.SetFractalType(FastNoiseLite::FractalType_Ridged);
		mtnN.SetFractalOctaves(5);
		mtnN.SetFrequency(1.8f);
		mtnN.SetFractalGain(0.5f);

		FastNoiseLite baseN;
		baseN.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
		baseN.SetFractalType(FastNoiseLite::FractalType_FBm);
		baseN.SetFractalOctaves(1);
		baseN.SetFrequency(1.0f);
		baseN.SetFractalGain(0.5f);

		std::vector<Vertex> vertices;
		std::vector<UINT> indices;

		for (int x = -1; x < res + 1; x++) {
			for (int y = -1; y < res + 1; y++) {
				XMFLOAT2 percent = { clamp(x / (res - 1.0f), 0.0f, 1.0f), clamp(y / (res - 1.0f), 0.0f, 1.0f) };
				XMFLOAT2 uv = { lerp(uvMin.x, uvMax.x, percent.x), lerp(uvMin.y, uvMax.y, percent.y) };
				XMVECTOR cubePos = CubePos(face, uv);

				XMFLOAT3 spherePos;
				XMStoreFloat3(&spherePos, XMVector3Normalize(cubePos));
				float base = baseN.GetNoise(spherePos.x, spherePos.y, spherePos.z) * 0.5f + 0.5f;
				
				float e = base;	

				float h = radius;

				if (x >= 0 && x < res && y >= 0 && y < res) vertices.push_back({ spherePos.x * h, spherePos.y * h, spherePos.z * h, 0.0f, 0.0f, 0.0f, e });
				else vertices.push_back({ spherePos.x * 0.99f * h, spherePos.y * 0.99f * h, spherePos.z * 0.99f * h, 0.0f, 0.0f, 0.0f, e });
			}
		}

		for (int x = 0; x < res + 1; x++) {
			for (int y = 0; y < res + 1; y++) {
				int i = x * (res + 2) + y;
				indices.push_back(i);
				indices.push_back(i + (res + 2));
				indices.push_back(i + (res + 2) + 1);

				indices.push_back(i);
				indices.push_back(i + (res + 2) + 1);
				indices.push_back(i + 1);
			}
		}

		for (size_t t = 0; t < indices.size(); t += 3) {

			UINT ia = indices[t], ib = indices[t + 1], ic = indices[t + 2];

			if (isSkirt(res, ia) || isSkirt(res, ib) || isSkirt(res, ic)) continue;

			XMVECTOR a = XMVectorSet(vertices[ia].x, vertices[ia].y, vertices[ia].z, 0.0f);
			XMVECTOR b = XMVectorSet(vertices[ib].x, vertices[ib].y, vertices[ib].z, 0.0f);
			XMVECTOR c = XMVectorSet(vertices[ic].x, vertices[ic].y, vertices[ic].z, 0.0f);

			
			XMVECTOR faceN = XMVector3Cross(b - a, c - a);
			XMFLOAT3 fn; XMStoreFloat3(&fn, faceN);

			vertices[ia].nx += fn.x; vertices[ia].ny += fn.y; vertices[ia].nz += fn.z;
			vertices[ib].nx += fn.x; vertices[ib].ny += fn.y; vertices[ib].nz += fn.z;
			vertices[ic].nx += fn.x; vertices[ic].ny += fn.y; vertices[ic].nz += fn.z;
		}

		ChunkData chunkData = { vertices, indices };

		return chunkData;

	}

	void UpdateLOD(ID3D11Device* device  ,node& n, XMFLOAT3 camPos, XMFLOAT3 center, float radius, ThreadPool& pool) {
		
		XMFLOAT3 localCamPos = XMFLOAT3{camPos.x - center.x, camPos.y - center.y, camPos.z - center.z};

		static UINT MAX = 15;
		float chunkSize = (2.0f * radius) / (1 << n.level);
		float threshold = 2.0f * chunkSize;
		float thresholdSq = threshold * threshold;


		float dx = (std::max)({ n.AABBMin.x - localCamPos.x, 0.0f, localCamPos.x - n.AABBMax.x });
		float dy = (std::max)({ n.AABBMin.y - localCamPos.y, 0.0f, localCamPos.y - n.AABBMax.y });
		float dz = (std::max)({ n.AABBMin.z - localCamPos.z, 0.0f, localCamPos.z - n.AABBMax.z });

		float distSq = dx * dx + dy * dy + dz * dz;

		if (isLeaf(n)) {
			if (n.pendingSplit) {
				for (int i = 0; i < 4; i++) {
					if (n.pendingChildren[i].wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
						return;
					}
				}
				for (int i = 0; i < 4; i++) {
					n.children[i] = std::make_unique<node>(BuildNode(n.face, n.quadMin(i), n.quadMax(i), n.level + 1, radius));
					n.children[i]->chunkData = n.pendingChildren[i].get();
					n.children[i]->AABBMin = { n.children[i]->chunkData.vertices[0].x, n.children[i]->chunkData.vertices[0].y, n.children[i]->chunkData.vertices[0].z};
					n.children[i]->AABBMax = n.children[i]->AABBMin;
					for (auto& v : n.children[i]->chunkData.vertices) {
						n.children[i]->AABBMin.x = (std::min)(v.x, n.children[i]->AABBMin.x); n.children[i]->AABBMin.y = (std::min)(v.y, n.children[i]->AABBMin.y); n.children[i]->AABBMin.z = (std::min)(v.z, n.children[i]->AABBMin.z);
						n.children[i]->AABBMax.x = (std::max)(v.x, n.children[i]->AABBMax.x); n.children[i]->AABBMax.y = (std::max)(v.y, n.children[i]->AABBMax.y); n.children[i]->AABBMax.z = (std::max)(v.z, n.children[i]->AABBMax.z);
					}

					UploadNode(device, *n.children[i]);
				}
				n.pendingSplit = false;
			}
			else if (n.level < MAX && distSq < thresholdSq) {
				for (int i = 0; i < 4; i++) {
					n.pendingChildren[i] = pool.enqueue( GenerateChunk, n.face, n.quadMin(i), n.quadMax(i), radius);
					n.pendingSplit = true;
				}
			}
		}
		else {
			if (distSq > thresholdSq * 1.2f) {
				for (auto& c : n.children) {
					c = nullptr;
				}
			}
			else {
				for (auto& c : n.children) {
					UpdateLOD(device, *c, camPos, center, radius, pool);
				}
			}
		}
	}

}
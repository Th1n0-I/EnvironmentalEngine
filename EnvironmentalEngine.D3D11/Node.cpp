#include "pch.h"
#include "Node.h"
#include "FastNoiseLite.h"
#include "MathHelper.h"


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

	chunkData GenerateChunk(UINT face, XMFLOAT2 uvMin, XMFLOAT2 uvMax) {

		UINT res = 16;

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

		for (UINT x = 0; x < res; x++) {
			for (UINT y = 0; y < res; y++) {
				XMFLOAT2 percent = { x / (res - 1.0f), y / (res - 1.0f) };
				XMFLOAT2 uv = { lerp(uvMin.x, uvMax.x, percent.x), lerp(uvMin.y, uvMax.y, percent.y) };
				XMVECTOR cubePos = CubePos(face, uv);

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

				float h = 1000.0f * (1.0f + strength * land);

				vertices.push_back({ spherePos.x * h, spherePos.y * h, spherePos.z * h, 0.0f, 0.0f, 0.0f, e });
			}
		}

		for (UINT x = 0; x < res - 1; x++) {
			for (UINT y = 0; y < res - 1; y++) {
				int i = x + y * res;
				indices.push_back(i);
				indices.push_back(i + res);
				indices.push_back(i + res + 1);

				indices.push_back(i);
				indices.push_back(i + res + 1);
				indices.push_back(i + 1);
			}
		}

		for (size_t t = 0; t < indices.size(); t += 3) {
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

		chunkData chunkData = { vertices, indices };

		return chunkData;

	}

	void UpdateLOD(ID3D11Device* device  ,node& n, XMFLOAT3 camPos) {
		
		static UINT MAX = 8;
		float chunkSize = (2.0f * 1000.0f) / (1 << n.level);
		float threshold = 3 * chunkSize;
		float thresholdSq = threshold * threshold;


		float dx = (std::max)({ n.AABBMin.x - camPos.x, 0.0f, camPos.x - n.AABBMax.x });
		float dy = (std::max)({ n.AABBMin.y - camPos.y, 0.0f, camPos.y - n.AABBMax.y });
		float dz = (std::max)({ n.AABBMin.z - camPos.z, 0.0f, camPos.z - n.AABBMax.z });

		float distSq = dx * dx + dy * dy + dz * dz;

		if (isLeaf(n)) {
			if (n.level < MAX && distSq < thresholdSq) {
				for (int i = 0; i < 4; i++) {
					n.children[i] = std::make_unique<node>(MakeNode(n.face, n.quadMin(i), n.quadMax(i), n.level + 1));
					UploadNode(device, *n.children[i]);
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
					UpdateLOD(device, *c, camPos);
				}
			}
		}
	}

}
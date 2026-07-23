#pragma once

#include "DirectXMath.h"
#include "Node.h"
#include <memory>
#include <d3d11.h>

namespace EnvironmentalEngine {
	class PlanetRenderer {
	public:
		DirectX::XMFLOAT3 center = { 0.0f, 0.0f, 0.0f };
		float radius = 1000.0f;
		float innerRadius = 1000.0f, outerRadius = 1025.0f, scaleHeight = 3.0f, sunIntensity = 20.0f;
		DirectX::XMFLOAT3 rayleighCoeff = { 0.0232f, 0.0540f, 0.1324f };


		std::unique_ptr<node> roots[6];

		PlanetRenderer(ID3D11Device* device, DirectX::XMFLOAT3 c, float r) : center(c), radius(r) {
			innerRadius = r;
			outerRadius = r * 1.025f;

			scaleHeight = r * 0.003f;
			rayleighCoeff = { 23.2f / r, 54.0f / r, 132.4f / r };

			for (UINT f = 0; f < 6; f++) {
				roots[f] = std::make_unique<node>(
					MakeNode(f, { 0.0f, 0.0f }, { 1.0f, 1.0f }, 0, radius)
				);
				UploadNode(device, *roots[f]);
			}
		}

		void Update(ID3D11Device* device, DirectX::XMFLOAT3 camPos) {
			for (UINT f = 0; f < 6; f++) {
				UpdateLOD(device, *roots[f], camPos, center, radius);
			}
		}
	};
}
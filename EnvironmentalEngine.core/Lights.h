#pragma once
#include <DirectXMath.h>

namespace EnvironmentalEngine {
	struct DirectionalLight {
	public:
		DirectX::XMFLOAT3 direction = { 0.0f, -1.0f, 0.0f };
		DirectX::XMFLOAT3 color = { 1.0f, 1.0f, 1.0f };
		float intensity = 1.0f;
	};
}
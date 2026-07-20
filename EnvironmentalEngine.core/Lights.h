#pragma once
#include <DirectXMath.h>

namespace EnvironmentalEngine {
	struct DirectionalLight {
		DirectX::XMFLOAT3 direction = { 0.0f, -1.0f, 0.0f };
		DirectX::XMFLOAT3 color = { 1.0f, 1.0f, 1.0f };
		float intensity = 1.0f;
	};
	struct AmbientLight {
		DirectX::XMFLOAT3 color = { 1.0f, 1.0f, 1.0f };
		float intensity = 0.1f;
	};
	struct PointLight {
		DirectX::XMFLOAT3 color = { 1.0f, 1.0f, 1.0f };
		DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
		float intensity = 1.0f;
	};
}
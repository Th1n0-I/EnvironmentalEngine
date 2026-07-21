#pragma once
#include <DirectXMath.h>
#include "Component.h"

namespace EnvironmentalEngine {
	struct DirectionalLight : public Component {
		const char* TypeName() const override { return "Directional light"; }
		DirectX::XMFLOAT3 direction = { 0.0f, -1.0f, 0.0f };
		DirectX::XMFLOAT3 color = { 1.0f, 1.0f, 1.0f };
		float intensity = 1.0f;
	};
	struct AmbientLight : public Component{
		const char* TypeName() const override { return "Ambient light"; }
		DirectX::XMFLOAT3 color = { 1.0f, 1.0f, 1.0f };
		float intensity = 0.1f;
	};
	struct PointLight : public Component {
		const char* TypeName() const override { return "Point light"; }
		DirectX::XMFLOAT3 color = { 1.0f, 1.0f, 1.0f };
		DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
		float intensity = 1.0f;
	};
}
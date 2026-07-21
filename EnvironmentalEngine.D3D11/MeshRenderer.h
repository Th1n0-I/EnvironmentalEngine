#pragma once

#include "Mesh.h"
#include "Component.h"
#include <DirectXMath.h>
#include <string>

namespace EnvironmentalEngine {
	struct MeshRenderer : public Component {
		const char* TypeName() const override { return "Mesh Renderer"; }


		Mesh* mesh{ nullptr };
		DirectX::XMFLOAT3 color{ 0.5f, 0.5f, 0.5f };
		float smoothness{ 0.5f };
		float specularIntensity{ 0.5f };
	};
}
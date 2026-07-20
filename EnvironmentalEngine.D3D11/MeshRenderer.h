#pragma once

#include "Mesh.h"
#include <DirectXMath.h>

namespace EnvironmentalEngine {
	struct MeshRenderer {
		Mesh* mesh;
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 rotation;
		DirectX::XMFLOAT3 scale;
		DirectX::XMFLOAT3 color;
		float smoothness;
		float specularIntensity;
	};
}
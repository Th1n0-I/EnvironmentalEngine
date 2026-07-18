#pragma once
#include <DirectXMath.h>
#include "Input.h"

namespace EnvironmentalEngine {

	class Camera {
	public:
		DirectX::XMMATRIX GetViewMatrix() const;

		DirectX::XMVECTOR GetForward() const;
		DirectX::XMVECTOR GetRight() const;

		void Update(const Input& input, float deltaTime);

		DirectX::XMFLOAT3 position = { 0.0f, 0.0f, -3.0f };
		float yaw = 0.0f;
		float pitch = 0.0f;
		float moveSpeed = 3.0f;
	};
}
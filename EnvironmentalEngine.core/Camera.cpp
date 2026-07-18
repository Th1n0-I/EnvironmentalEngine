#include "pch.h"
#include "Camera.h"

using namespace DirectX;

namespace EnvironmentalEngine {
	XMVECTOR Camera::GetForward() const {
		float cosPitch = cosf(pitch);
		XMVECTOR forward = XMVectorSet(
			cosPitch * sinf(yaw),
			sinf(pitch),
			cosPitch * cosf(yaw),
			0.0f);
		return XMVector3Normalize(forward);
	}

	XMVECTOR Camera::GetRight() const {
		XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		return XMVector3Normalize(XMVector3Cross(worldUp, GetForward()));
	}

	XMMATRIX Camera::GetViewMatrix() const {
		XMVECTOR eye = XMLoadFloat3(&position);
		XMVECTOR target = eye + GetForward();
		XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		return XMMatrixLookAtLH(eye, target, up);
	}
}
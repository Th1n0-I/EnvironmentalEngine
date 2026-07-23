#pragma once
#include <algorithm>

namespace EnvironmentalEngine {
	inline float clamp(float v, float minValue, float maxValue) {
		return (std::max)((std::min)(v, maxValue), minValue);
	}

	inline float lerp(float a, float b, float t) {
		return a + t * (b - a);
	}

	inline float smoothstep(float edge0, float edge1, float x) {

		x = clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);

		return x * x * (3 - 2 * x);
	}
}
#pragma once
#include <chrono>

namespace EnvironmentalEngine {
	class Timer {
	public:
		Timer();

		void Tick();
		float DeltaTime() const { return m_delta; }
		float TotalTime() const { return m_total; }

	private:
		std::chrono::steady_clock::time_point m_start;
		std::chrono::steady_clock::time_point m_last;
		float m_delta = 0.0f;
		float m_total = 0.0f;
};
}
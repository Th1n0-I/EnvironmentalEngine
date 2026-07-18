#include "pch.h"
#include "Timer.h"

namespace EnvironmentalEngine {
	Timer::Timer() {
		m_start = std::chrono::steady_clock::now();
		m_last = m_start;
	}

	void Timer::Tick() {
		auto now = std::chrono::steady_clock::now();

		std::chrono::duration<float> delta = now - m_last;
		std::chrono::duration<float> total = now - m_start;

		m_delta = delta.count();
		m_total = total.count();

		m_last = now;
	}
}
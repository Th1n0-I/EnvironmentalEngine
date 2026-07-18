#pragma once

namespace EnvironmentalEngine {
	class Input {
	public:
		void OnKeyDown(int key) { m_keys[key] = true; }
		void OnKeyUp(int key) { m_keys[key] = false; }

		bool IsKeyDown(int key) const { return m_keys[key]; }

	private:
		bool m_keys[256] = {};
	};
}
#pragma once

namespace EnvironmentalEngine {
	class Input {
	public:
		void OnMouseMove(int x, int y) { m_mouseX = x; m_mouseY = y; }

		void onRightDown() { m_rightDown = true; }
		void onRightUp() { m_rightDown = false; }

		bool isRightDown() const { return m_rightDown; }
		int mouseX() const { return m_mouseX; }
		int mouseY() const { return m_mouseY; }

		void OnKeyDown(int key) { m_keys[key] = true; }
		void OnKeyUp(int key) { m_keys[key] = false; }

		bool IsKeyDown(int key) const { return m_keys[key]; }

	private:
		bool m_keys[256] = {};
		int m_mouseX = 0, m_mouseY = 0;
		bool m_rightDown = false;
	};
}
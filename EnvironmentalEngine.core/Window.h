#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMIMAX
#define NOMIMAX
#endif
#include <Windows.h>
#include <string>

namespace EnvironmentalEngine {
	class Window {
	public:
		Window(const std::wstring& title, int width, int height);
		~Window();

		Window(const Window&) = delete;
		Window& operator = (const Window&) = delete;

		bool ProccessMessages();

		HWND Handle() const { return m_hwnd; }
		int Width() const { return m_width; }
		int Height() const { return m_height; }

	private:
		static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
		LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

		HWND m_hwnd = nullptr;
		int m_width = 0;
		int m_height = 0;
	};
}
#include "pch.h"
#include "Window.h"
#include <stdexcept>

namespace EnvironmentalEngine {
	namespace {
		constexpr const wchar_t* kClassName = L"EnvironmentalEngineWindow";
	}

	Window::Window(const std::wstring& title, int width, int height)
		: m_width(width), m_height(height) {
		const HINSTANCE instance = GetModuleHandleW(nullptr);

		WNDCLASSEXW wc = {};
		wc.cbSize = sizeof(wc);
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = WndProc;
		wc.hInstance = instance;
		wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
		wc.lpszClassName = kClassName;
		RegisterClassExW(&wc);

		RECT rect = { 0, 0, width, height };
		AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

		CreateWindowExW(
			0,
			kClassName,
			title.c_str(),
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT,
			rect.right - rect.left,
			rect.bottom - rect.top,
			nullptr, nullptr,
			instance,
			this);

		if (!m_hwnd) throw std::runtime_error("Failed to create window!");

		ShowWindow(m_hwnd, SW_SHOW);
	}

	Window::~Window() {
		if (m_hwnd) DestroyWindow(m_hwnd);

		UnregisterClassW(kClassName, GetModuleHandleW(nullptr));
	}

	bool Window::ProccessMessages() {
		MSG msg = {};
		while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) return false;

			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
		return true;
	}

	LRESULT CALLBACK Window::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		Window* self = nullptr;

		if (msg == WM_NCCREATE) {
			auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
			self = static_cast<Window*>(cs->lpCreateParams);
			SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
			self->m_hwnd = hwnd;
		}
		else {
			self = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
		}

		if (self) return self->HandleMessage(msg, wParam, lParam);

		return DefWindowProcW(hwnd, msg, wParam, lParam);
	}

	LRESULT Window::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
		switch (msg) {
		case WM_SIZE:
			m_width = LOWORD(lParam);
			m_height = HIWORD(lParam);
			return 0;

		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		}

		return DefWindowProcW(m_hwnd, msg, wParam, lParam);
	}
}
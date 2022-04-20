#pragma once

#include <Windows.h>

#include "resource.h"

#define IDM_FILE_OPEN 0
#define IDM_FILE_CLOSE 1

#define UWM_FILE_OPEN (WM_USER + 0)

#define UWM_IDX(X) ((X) - 0x0400)

class Window {
public:
	Window(HINSTANCE, const wchar_t*);
	~Window() { }

	static void Register(HBRUSH background, HINSTANCE instance, const wchar_t* class_name);
	
	virtual void Create() { }
	virtual void Show();

	HWND getWindowHandle();

private:
	static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMessage, WPARAM wParam, LPARAM lParam);

protected:
	virtual LRESULT OnCreate(WPARAM wParam, LPARAM lParam) { 
		return DefWindowProc(this->hwnd, WM_CREATE, wParam, lParam);
	}
	virtual LRESULT OnPaint(WPARAM wParam, LPARAM lParam) { 
		return DefWindowProc(this->hwnd, WM_PAINT, wParam, lParam);
	}
	virtual LRESULT OnCommand(WPARAM wParam, LPARAM lParam) { 
		return DefWindowProc(this->hwnd, WM_COMMAND, wParam, lParam);
	}
	virtual LRESULT OnMouseClick(WPARAM wParam, LPARAM lParam) { 
		return DefWindowProc(this->hwnd, WM_LBUTTONDOWN, wParam, lParam);
	}
	virtual LRESULT OnMouseMove(WPARAM wParam, LPARAM lParam) { 
		return DefWindowProc(this->hwnd, WM_MOUSEMOVE, wParam, lParam);
	}
	virtual LRESULT OnMouseLeave(WPARAM wParam, LPARAM lParam) { 
		return DefWindowProc(this->hwnd, WM_MOUSELEAVE, wParam, lParam);
	}
	virtual LRESULT OnDestroy(WPARAM wParam, LPARAM lParam) {
		PostQuitMessage(0);
		return DefWindowProc(this->hwnd, WM_DESTROY, wParam, lParam);
	}
	
	virtual LRESULT OnCustomMessage(int idx, WPARAM wParam, LPARAM lParam) {
		return DefWindowProc(this->hwnd, idx + 0x400, wParam, lParam);
	}

	HINSTANCE instance;
	HWND hwnd;

	wchar_t class_name[32];
};

Window::Window(HINSTANCE hInstance, const wchar_t* class_name) {
	this->instance = hInstance;
	this->hwnd = NULL;

	wcscpy_s(this->class_name, 32, class_name);
}

void Window::Register(HBRUSH background, HINSTANCE instance, const wchar_t* class_name) {
	WNDCLASS wc = { 0, };

	wc.cbClsExtra    = NULL;
	wc.cbWndExtra    = sizeof(Window*);
	wc.hbrBackground = background;
	wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon         = LoadIcon(instance, MAKEINTRESOURCE(IDI_ICON));
	wc.hInstance     = instance;
	wc.lpfnWndProc   = WindowProc;
	wc.lpszClassName = class_name;
	wc.lpszMenuName  = NULL;
	wc.style         = CS_HREDRAW | CS_VREDRAW;
	
	RegisterClass(&wc);
}
void Window::Show() {
	ShowWindow(hwnd, SW_SHOW);
	UpdateWindow(hwnd);
}

HWND Window::getWindowHandle() {
	return hwnd;
}

LRESULT CALLBACK Window::WindowProc(HWND hWnd, UINT uMessage, WPARAM wParam, LPARAM lParam) {
	Window* window = nullptr;

	if (uMessage == WM_CREATE) {
		window = (Window*)((CREATESTRUCT*)lParam)->lpCreateParams;
		window->hwnd = hWnd;

		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)window);
	}
	else {
		window = (Window*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	}

	switch (uMessage) {
	case WM_CREATE:      return window->OnCreate(wParam, lParam);
	case WM_PAINT:       return window->OnPaint(wParam, lParam);
	case WM_COMMAND:     return window->OnCommand(wParam, lParam);
	case WM_LBUTTONDOWN: return window->OnMouseClick(wParam, lParam);
	case WM_MOUSEMOVE:   return window->OnMouseMove(wParam, lParam);
	case WM_MOUSELEAVE:  return window->OnMouseLeave(wParam, lParam);
	case WM_DESTROY:     return window->OnDestroy(wParam, lParam);
	}

	if (uMessage >= UWM_FILE_OPEN) {
		return window->OnCustomMessage(UWM_IDX(uMessage), wParam, lParam);
	}

	return DefWindowProc(hWnd, uMessage, wParam, lParam);
}
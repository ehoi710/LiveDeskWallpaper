#pragma once

#include <thread>
#include <string>

#include "Window.h"
#include "MediaProvider.h"

#define SUB_X(i) (10 + (sub_width + 10) * (i))
#define SUB_Y(i) (10 + (sub_height + 10) * (i))

#define SUB_COL 3
#define SUB_ROW 3
#define MAX_SUBWINDOW (SUB_ROW * SUB_COL)

const int sub_width = 256;
const int sub_height = 144;

extern std::thread subwindow_thread;
extern bool subwindow_condition;

bool tracking = false;

class RecentlyQueue {
public:
	void Push(std::wstring file_name);

	const std::wstring& operator[](int index);

private:
	std::wstring recently_queue[MAX_SUBWINDOW];
};

void RecentlyQueue::Push(std::wstring file_name) {
	int i, find;

	for (find = 0; find < MAX_SUBWINDOW; find++) {
		if (recently_queue[find] == file_name) break; 
	}

	for (i = min(MAX_SUBWINDOW - 1, find); i > 0; i--)
		recently_queue[i] = recently_queue[i - 1];

	recently_queue[0] = file_name;
}
const std::wstring& RecentlyQueue::operator[](int index) {
	return recently_queue[index];
}

RecentlyQueue recently_queue;

class ChildWindow : public Window {
public:
	ChildWindow(HINSTANCE hInstance, Window* window);
	~ChildWindow();

	static void Register(HINSTANCE instance);

	virtual void Create() override;

	void SetVideo(std::wstring file_name);

protected:
	virtual LRESULT OnPaint(WPARAM, LPARAM) override;
	virtual LRESULT OnMouseMove(WPARAM, LPARAM) override;
	virtual LRESULT OnMouseClick(WPARAM, LPARAM) override;
	virtual LRESULT OnMouseLeave(WPARAM, LPARAM) override;

private:
	int id;

	static const wchar_t* class_name;

	HWND parent;
	MediaProvider* provider;
};

const wchar_t* ChildWindow::class_name = L"subwindow";

ChildWindow::ChildWindow(HINSTANCE hInstance, Window* window) : Window(hInstance, L"subwindow") {
	static int i = 0;

	id = i; i++;

	provider = nullptr;

	parent = window->getWindowHandle();
}
ChildWindow::~ChildWindow() {
	if (provider != nullptr)
		delete provider;
}

void ChildWindow::Register(HINSTANCE instance) {
	Window::Register(CreateSolidBrush(RGB(0, 0, 0)), instance, ChildWindow::class_name);
}

void ChildWindow::Create() {
	CreateWindowExW(WS_EX_ACCEPTFILES, this->class_name, L"", WS_CHILD, SUB_X(id%SUB_COL), SUB_Y(id/SUB_COL), 
		sub_width, sub_height, parent, (HMENU)id, this->instance, this);
	provider = new MediaProvider(hwnd);
}

void ChildWindow::SetVideo(std::wstring file_name) {
	provider->clear();

	if (file_name != L"")
		provider->open(file_name.c_str());
		
	provider->fetchOne();
}

LRESULT ChildWindow::OnPaint(WPARAM, LPARAM) {
	provider->fetchOne();
	return 0;
}
LRESULT ChildWindow::OnMouseMove(WPARAM, LPARAM) {
	TRACKMOUSEEVENT tme = {
		sizeof(TRACKMOUSEEVENT),
		TME_LEAVE,
		hwnd,
		0
	};

	if (!tracking) {
		tracking = true;

		subwindow_condition = false;
		subwindow_thread = std::thread([=]() { provider->drawLoop(&subwindow_condition); });

		TrackMouseEvent(&tme);
	}
	return 0;
}
LRESULT ChildWindow::OnMouseClick(WPARAM, LPARAM) {
	if (recently_queue[id] != L"")
		SendMessage(parent, WM_USER, (WPARAM)recently_queue[id].c_str(), 0);
	else
		MessageBox(0, L"NULL", L"", 0);

	return 0;
}
LRESULT ChildWindow::OnMouseLeave(WPARAM, LPARAM) {
	if (tracking) {
		tracking = false;

		subwindow_condition = true;
		if (subwindow_thread.joinable())
			subwindow_thread.join();

		provider->fetchOne();
	}

	return 0;
}
#pragma once

#include <thread>
#include <string.h>

#include "Window.h"
#include "ChildWindow.h"
#include "MediaProvider.h"
#include "GlobalFunctions.h"

extern std::thread wallpaper_thread;
extern bool wallpaper_condition;

class MainWindow : public Window {
public:
	MainWindow(HINSTANCE hInstance);
	~MainWindow();

	void OpenFile(std::wstring file_name);
	void RunWallpaperThread();

	static void Register(HINSTANCE instance);
	
	virtual void Create();
	virtual void Run();

	int getExitStatus();

protected:
	virtual LRESULT OnCreate(WPARAM, LPARAM) override;
	virtual LRESULT OnCommand(WPARAM wParam, LPARAM) override;

	virtual LRESULT OnCustomMessage(int idx, WPARAM wParam, LPARAM lParam) override;

	int exit_status;

private:
	void SetWallpaper(const wchar_t* file_name);
	void RefreshChild();

	static const wchar_t* class_name;

	HWND wallpaper_handle;

	MediaProvider* wallpaper_provider;
	ChildWindow* child[9] = { 0, };
};

const wchar_t* MainWindow::class_name = L"LiveDeskWallpaperGUI";

MainWindow::MainWindow(HINSTANCE hInstance) : Window(hInstance, this->class_name) {
	wallpaper_handle = GetWallpaperWindow();
	wallpaper_provider = new MediaProvider(wallpaper_handle);

	this->exit_status = 0;
}
MainWindow::~MainWindow() {
	ShowWindow(wallpaper_handle, SW_SHOW);

	if (wallpaper_provider != nullptr)
		delete wallpaper_provider;
}

void MainWindow::OpenFile(std::wstring file_name) {
	// if condition is false, threads aren't running
	wallpaper_condition = true;
	subwindow_condition = true;
	
	// mouse tracking disable
	tracking = false;

	if (wallpaper_thread.joinable()) wallpaper_thread.join();
	if (subwindow_thread.joinable()) subwindow_thread.join();

	SetWallpaper(file_name.c_str());

	recently_queue.Push(file_name);

	RefreshChild();

	RunWallpaperThread();
}

void MainWindow::RunWallpaperThread() {
	wallpaper_condition = false; // if false, loop is running
	wallpaper_thread = std::thread([=]() { wallpaper_provider->drawLoop(&wallpaper_condition); });
}

void MainWindow::Register(HINSTANCE instance) {
	
	Window::Register((HBRUSH)COLOR_WINDOW, instance, MainWindow::class_name);
}
void MainWindow::Create() {
	CreateWindowExW(WS_EX_ACCEPTFILES, this->class_name, L"Live Desktop Wallpaper GUI", WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME, 
		160, 90, SUB_X(SUB_COL) + 16, SUB_Y(SUB_ROW) + 59, nullptr, nullptr, instance, this);
}
void MainWindow::Run() {
	MSG msg;
	wchar_t file_name[128];
	while (GetMessage(&msg, NULL, 0, 0)) {
		try {
			if (msg.message == WM_DROPFILES) {
				DragQueryFile((HDROP)msg.wParam, 0, file_name, 128);
				OpenFile(file_name);
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		catch(...) {
			PostQuitMessage(0);
		}
	}

	exit_status = (int)msg.wParam;
}

void MainWindow::SetWallpaper(const wchar_t* file_name) {
	wallpaper_provider->clear();
	wallpaper_provider->open(file_name);
}
int MainWindow::getExitStatus() {
	return exit_status;
}

LRESULT MainWindow::OnCreate(WPARAM wParam, LPARAM lParam) {
	HMENU hMenubar, hMenu;

	hMenubar = CreateMenu();
	hMenu = CreateMenu();

	AppendMenuW(hMenu, MF_STRING, IDM_FILE_OPEN, L"&Open");
	AppendMenuW(hMenubar, MF_POPUP, (UINT_PTR)hMenu, L"&File");

	SetMenu(hwnd, hMenubar);

	RECT rect;
	int client_width, client_height;

	GetClientRect(hwnd, &rect);

	client_width = rect.right - rect.left;
	client_height = rect.bottom - rect.top;

	for (int i = 0; i < MAX_SUBWINDOW; i++) {
		child[i] = new ChildWindow(instance, this);
		
		child[i]->Create();
		child[i]->Show();
	}

	return 0;
}
LRESULT MainWindow::OnCommand(WPARAM wParam, LPARAM lParam) {
	wchar_t file_name[128];

	switch (LOWORD(wParam)) {
	case IDM_FILE_OPEN:
		ShowFileOpenDialog(file_name);
		OpenFile(file_name);
		break;
	}

	return 0;
}

LRESULT MainWindow::OnCustomMessage(int idx, WPARAM wParam, LPARAM lParam) {
	switch (idx) {
	case 0: OpenFile((const wchar_t*)wParam);
	}
	
	return 0;
}

void MainWindow::RefreshChild() {
	for (int i = 0; i < MAX_SUBWINDOW; i++) {
		child[i]->SetVideo(recently_queue[i]);
	}
	return;
}
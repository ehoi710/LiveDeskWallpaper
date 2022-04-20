#include "MainWindow.h"

#undef main

std::thread wallpaper_thread; bool wallpaper_condition = true;
std::thread subwindow_thread; bool subwindow_condition = true;

int wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow) {
	SDL_Init(SDL_INIT_VIDEO);
	av_register_all();

	MainWindow::Register(hInstance);
	ChildWindow::Register(hInstance);

	MainWindow window(hInstance);
	window.Create();

	if (lpCmdLine[0] != '\0') window.OpenFile(lpCmdLine);
	
	window.Show();
	// window.Run();

	MSG msg;
	wchar_t file_name[128];
	
	try {
		while (GetMessage(&msg, NULL, 0, 0)) {
			if (msg.message == WM_DROPFILES) {
				DragQueryFile((HDROP)msg.wParam, 0, file_name, 128);
				window.OpenFile(file_name);
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	catch (...) {
		PostQuitMessage(0);
	}

	wallpaper_condition = true;
	subwindow_condition = true;

	if (wallpaper_thread.joinable()) wallpaper_thread.join();
	if (subwindow_thread.joinable()) subwindow_thread.join();

	SDL_Quit();

	return msg.wParam;
}
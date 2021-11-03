#include <thread>
#include <string>

#include <shobjidl.h>
#include <Windows.h>

#include "MediaProvider.h"

#undef main

#define IDM_FILE_OPEN 0
#define IDM_FILE_CLOSE 1

using namespace std::chrono;

LRESULT CALLBACK ChildProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

BOOL CALLBACK EnumWindowsProc(HWND, LPARAM);
HWND GetWallpaperWindow();

void RegisterMyWindow(HINSTANCE);

void SetFile(const wchar_t*);
void AddRecentlyQueue(const wchar_t*);
void RefreshChild();

int ShowFileOpenDialog(wchar_t* _dst);

const wchar_t* class_name = L"LiveDeskWallpaperGUI";
const wchar_t* title = L"Live Desktop Wallpaper GUI";

MediaProvider* wallpaper_provider;
MediaProvider* subwindow_provider[6];

HWND wallpaper_handle;
HWND subwindow_handle[6];

std::wstring recently_queue[6];

bool tracking = false;

bool wallpaper_check = true, subwindow_check = true;
std::thread wallpaper_thread, subwindow_thread;

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	HWND hWnd;
	MSG msg;
	RECT rect;

	int w, h;
	
	SDL_Init(SDL_INIT_VIDEO);
	av_register_all();

	RegisterMyWindow(hInstance);

	hWnd = CreateWindowW(class_name, title, WS_OVERLAPPEDWINDOW, 
		160, 90, 640, 360, NULL, NULL, hInstance, NULL);

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	wallpaper_handle = GetWallpaperWindow();
	wallpaper_provider = new MediaProvider(wallpaper_handle);
	
	GetClientRect(hWnd, &rect);
	
	w = rect.right - rect.left;
	h = rect.bottom - rect.top;

	for (int i = 0; i < 6; i++) {
		subwindow_handle[i] = CreateWindowExW(0, L"test", nullptr, WS_CHILD,
			10 + ((w - 10) / 3) * (i % 3), 
			10 + ((h - 10) / 2) * (i / 3), 
			((w - 40) / 3), 
			((h - 30) / 2), 
			hWnd, (HMENU)i, hInstance, nullptr);

		ShowWindow(subwindow_handle[i], nCmdShow);
		UpdateWindow(subwindow_handle[i]);

		subwindow_provider[i] = new MediaProvider(subwindow_handle[i]);
	}

	if (lpCmdLine[0] != '\0') {
		wallpaper_check = false;

		wallpaper_provider->open(lpCmdLine);
		wallpaper_thread = std::thread(
			[](MediaProvider* prov, const bool* check) {
				prov->drawLoop(check);
			},
			wallpaper_provider,
			&wallpaper_check
		);
	}

	while (GetMessage(&msg, NULL, 0, 0)) {
		try {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		catch(...) {
			PostQuitMessage(0);
		}
	}

	wallpaper_check = true;

	if (wallpaper_thread.joinable()) wallpaper_thread.join();

	delete wallpaper_provider;

	SDL_Quit();

	ShowWindow(wallpaper_handle, SW_SHOW);

	return (int)msg.wParam;
}

LRESULT CALLBACK ChildProc(HWND hWnd, UINT uMessage, WPARAM wParam, LPARAM lParam) {
	int id = GetWindowLong(hWnd, GWL_ID);

	switch (uMessage) {
	case WM_PAINT: {
		HDC hdc;
		PAINTSTRUCT ps;
		RECT rect;

		hdc = BeginPaint(hWnd, &ps);
		GetClientRect(hWnd, &rect);
		
		FillRect(
			hdc,
			&rect,
			CreateSolidBrush(RGB(0, 0, 0))
		);

		EndPaint(hWnd, &ps);
	} break;

	case WM_MOUSEMOVE: {
		TRACKMOUSEEVENT tme = {
			sizeof(TRACKMOUSEEVENT),
			TME_LEAVE,
			hWnd,
			0
		};

		if (!tracking) {
			tracking = true;

			subwindow_check = false;
			subwindow_thread = std::thread(
				[](MediaProvider* prov, const bool* check) {
					prov->drawLoop(check);
				},
				subwindow_provider[id],
				&subwindow_check
			);

			TrackMouseEvent(&tme);
		}
	} break;

	case WM_MOUSELEAVE: {
		if (tracking) {
			tracking = false;

			subwindow_check = true;
			if (subwindow_thread.joinable()) subwindow_thread.join();

			subwindow_provider[id]->fetchOne();
		}
	} break;

	case WM_LBUTTONDOWN: {
		int id = GetWindowLong(hWnd, GWL_ID);
		if (recently_queue[id] != L"")
			MessageBox(0, recently_queue[id].c_str(), L"", 0);
		else
			MessageBox(0, L"NULL", L"", 0);
	} break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	}

	return DefWindowProc(hWnd, uMessage, wParam, lParam);
}
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMessage, WPARAM wParam, LPARAM lParam) {
	wchar_t file_name[128];

	switch (uMessage) {
	case WM_CREATE: {
		HMENU hMenubar, hMenu;

		hMenubar = CreateMenu();
		hMenu = CreateMenu();

		AppendMenuW(hMenu, MF_STRING, IDM_FILE_OPEN, L"&Open");
		AppendMenuW(hMenubar, MF_POPUP, (UINT_PTR)hMenu, L"&File");

		SetMenu(hWnd, hMenubar);
		DragAcceptFiles(hWnd, true);
	} break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDM_FILE_OPEN:
			ShowFileOpenDialog(file_name);
			SetFile(file_name);
			break;
		}
		break;

	case WM_DROPFILES: 
		DragQueryFile((HDROP)wParam, 0, file_name, 128);
		SetFile(file_name);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	}

	return DefWindowProc(hWnd, uMessage, wParam, lParam);
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lparam) {
	HWND p, *ret;

	p = FindWindowEx(hwnd, NULL, L"SHELLDLL_DefView", NULL);
	ret = (HWND*)lparam;

	if (p) *ret = FindWindowEx(NULL, hwnd, L"WorkerW", NULL);

	return true;
}
HWND GetWallpaperWindow() {
	HWND progman, wallpaper_hwnd;

	progman = FindWindow(L"progman", NULL);
	wallpaper_hwnd = nullptr;

	SendMessageTimeout(progman, 0x052C, 0, 0, SMTO_NORMAL, 1000, nullptr);

	EnumWindows(EnumWindowsProc, (LPARAM)&wallpaper_hwnd);
	return wallpaper_hwnd;
}

void RegisterMyWindow(HINSTANCE hInstance) {
	WNDCLASS wc = { 0, };
	WNDCLASS cwc = { 0, };

	wc.cbClsExtra = NULL;
	wc.cbWndExtra = NULL;
	wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hInstance = hInstance;
	wc.lpfnWndProc = WndProc;
	wc.lpszClassName = class_name;
	wc.lpszMenuName = NULL;
	wc.style = CS_HREDRAW | CS_VREDRAW;

	cwc.cbClsExtra = NULL;
	cwc.cbWndExtra = NULL;
	cwc.hbrBackground = CreateSolidBrush(RGB(0, 0, 0));
	cwc.hCursor = LoadCursor(NULL, IDC_ARROW);
	cwc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	cwc.hInstance = hInstance;
	cwc.lpfnWndProc = ChildProc;
	cwc.lpszClassName = L"test";
	cwc.lpszMenuName = NULL;
	cwc.style = CS_HREDRAW | CS_VREDRAW;

	RegisterClass(&wc);
	RegisterClass(&cwc);
}

void SetFile(const wchar_t* file_name) {
	wallpaper_check = true;

	if (wallpaper_thread.joinable()) wallpaper_thread.join();

	wallpaper_provider->clear();

	wallpaper_check = false;

	wallpaper_provider->open(file_name);
	wallpaper_thread = std::thread(
		[](MediaProvider* prov, const bool* check) {
			prov->drawLoop(check);
		},
		wallpaper_provider,
		&wallpaper_check
	);
	
	AddRecentlyQueue(file_name);
	RefreshChild();
}
void AddRecentlyQueue(const wchar_t* str) {
	int i, find;
	for (find = 0; find < 6; find++) {
		if (recently_queue[find] == str) break;
	}

	for (i = min(5, find); i > 0; i--)
		recently_queue[i] = recently_queue[i - 1];

	recently_queue[0] = std::wstring(str);
}
void RefreshChild() {
	for (int i = 0; i < 6; i++) {
		subwindow_provider[i]->clear();
		if (recently_queue[i] != L"") {
			subwindow_provider[i]->open(recently_queue[i].c_str());
		}
		subwindow_provider[i]->fetchOne();
	}
	return;
}

int ShowFileOpenDialog(wchar_t* _dst) {
	IFileOpenDialog* pFileOpen;
	IShellItem* pItem;
	PWSTR pszFilePath;
	HRESULT hr;

	if (!SUCCEEDED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE))) return 0;

	hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_IFileOpenDialog, (void**)&pFileOpen);
	if (SUCCEEDED(hr)) {
		hr = pFileOpen->Show(NULL);
		if (SUCCEEDED(hr)) {
			hr = pFileOpen->GetResult(&pItem);
			if (SUCCEEDED(hr)) {
				hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, 
						                    &pszFilePath);
				if (SUCCEEDED(hr)) {
					wcscpy(_dst, pszFilePath);
					CoTaskMemFree(pszFilePath);
				}
				pItem->Release();
			}
		}
		pFileOpen->Release();
	}
	CoUninitialize();

	return 0;
}
#include <thread>
#include <string>

#include <shobjidl.h>
#include <Windows.h>

#include "MediaProvider.h"

#undef main

#define IDM_FILE_OPEN 0
#define IDM_FILE_CLOSE 1

#define SUB_X(i) (10 + (sub_width + 10) * (i))
#define SUB_Y(i) (10 + (sub_height + 10) * (i))

#define SUB_COL 3
#define SUB_ROW 3
#define MAX_SUBWINDOW (SUB_ROW * SUB_COL)

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

const wchar_t* subwindow_class_name = L"sub window";

MediaProvider* wallpaper_provider;
MediaProvider* subwindow_provider[MAX_SUBWINDOW];

HWND wallpaper_handle;
HWND subwindow_handle[MAX_SUBWINDOW];

std::wstring recently_queue[MAX_SUBWINDOW];

bool tracking = false;

bool wallpaper_check = true, subwindow_check = true;
std::thread wallpaper_thread, subwindow_thread;

const int sub_width = 256;
const int sub_height = 144;

int wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
	HWND hWnd;
	MSG msg;
	RECT rect;

	wchar_t file_name[128];

	int client_width, client_height;
	
	SDL_Init(SDL_INIT_VIDEO);
	av_register_all();

	RegisterMyWindow(hInstance);
	
	hWnd = CreateWindowExW(WS_EX_ACCEPTFILES, class_name, title, WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME, 
		160, 90, SUB_X(SUB_COL) + 16, SUB_Y(SUB_ROW) + 59, nullptr, nullptr, hInstance, nullptr);

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	wallpaper_handle = GetWallpaperWindow();
	wallpaper_provider = new MediaProvider(wallpaper_handle);
	
	GetClientRect(hWnd, &rect);
	
	client_width = rect.right - rect.left;
	client_height = rect.bottom - rect.top;

	for (int i = 0; i < MAX_SUBWINDOW; i++) {
		subwindow_handle[i] = CreateWindowExW(WS_EX_ACCEPTFILES, subwindow_class_name, L"", WS_CHILD,
			SUB_X(i%SUB_COL), SUB_Y(i/SUB_COL), sub_width, sub_height, hWnd, (HMENU)i, hInstance, nullptr);
		subwindow_provider[i] = new MediaProvider(subwindow_handle[i]);

		ShowWindow(subwindow_handle[i], nCmdShow);
		UpdateWindow(subwindow_handle[i]);
	}

	if (lpCmdLine[0] != '\0') {
		SetFile(lpCmdLine);
	}

	while (GetMessage(&msg, NULL, 0, 0)) {
		try {
			if (msg.message == WM_DROPFILES) {
				DragQueryFile((HDROP)msg.wParam, 0, file_name, 128);
				SetFile(file_name);
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		catch(...) {
			PostQuitMessage(0);
		}
	}

	wallpaper_check = true;

	if (wallpaper_thread.joinable()) 
		wallpaper_thread.join();

	delete wallpaper_provider;

	SDL_Quit();

	ShowWindow(wallpaper_handle, SW_SHOW);

	return (int)msg.wParam;
}

LRESULT CALLBACK ChildProc(HWND hWnd, UINT uMessage, WPARAM wParam, LPARAM lParam) {
	int id = GetWindowLong(hWnd, GWL_ID);
	wchar_t file_name[128];
	
	switch (uMessage) {
	case WM_CREATE: {
		// DragAcceptFiles(hWnd, true);
	} break;

	case WM_PAINT: {
		subwindow_provider[id]->fetchOne();
	} break;

	/*case WM_DROPFILES: {
		DragQueryFile((HDROP)wParam, 0, file_name, 128);
		SetFile(file_name);
	} break;*/

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
			if (subwindow_thread.joinable()) 
				subwindow_thread.join();

			subwindow_provider[id]->fetchOne();
		}
	} break;

	case WM_LBUTTONDOWN: {
		if (recently_queue[id] != L"")
			SetFile(recently_queue[id].c_str());
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
		// DragAcceptFiles(hWnd, true);
	} break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDM_FILE_OPEN:
			ShowFileOpenDialog(file_name);
			SetFile(file_name);
			break;
		}
		break;

	/*case WM_DROPFILES: {
		DragQueryFile((HDROP)wParam, 0, file_name, 128);
		SetFile(file_name);
	} break;*/

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
	cwc.lpszClassName = subwindow_class_name;
	cwc.lpszMenuName = NULL;
	cwc.style = CS_HREDRAW | CS_VREDRAW;

	RegisterClass(&wc);
	RegisterClass(&cwc);
}

void SetFile(const wchar_t* file_name) {
	wallpaper_check = true;
	subwindow_check = true;

	if (wallpaper_thread.joinable()) 
		wallpaper_thread.join();

	if (subwindow_thread.joinable()) 
		subwindow_thread.join();

	wallpaper_provider->clear();
	wallpaper_check = false;
	
	subwindow_check = false;
	tracking = false;
	
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
	std::wstring new_str(str);

	for (find = 0; find < MAX_SUBWINDOW; find++) {
		if (recently_queue[find] == str) break;
	}

	for (i = min(MAX_SUBWINDOW - 1, find); i > 0; i--)
		recently_queue[i] = recently_queue[i - 1];

	recently_queue[0] = new_str;
}
void RefreshChild() {
	for (int i = 0; i < MAX_SUBWINDOW; i++) {
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

	if (!SUCCEEDED(CoInitializeEx(NULL, COINIT_DISABLE_OLE1DDE | COINIT_APARTMENTTHREADED))) return 0;
	if (!SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_IFileOpenDialog, (void**)&pFileOpen))) goto E1;
	if (!SUCCEEDED(pFileOpen->Show(NULL)) || !SUCCEEDED(pFileOpen->GetResult(&pItem))) goto E2;
	if (!SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath))) goto E3;

	wcscpy(_dst, pszFilePath);
	CoTaskMemFree(pszFilePath);

E3:	pItem->Release();
E2:	pFileOpen->Release();
E1:	CoUninitialize();

	return 0;
}
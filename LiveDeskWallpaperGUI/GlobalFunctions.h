#pragma once

#include <Windows.h>
#include <shobjidl.h>

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

int ShowFileOpenDialog(wchar_t* _dst) {
	IFileOpenDialog* pFileOpen = nullptr;
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
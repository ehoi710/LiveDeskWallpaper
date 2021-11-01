extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libavutil/avutil.h>
	#include <libavutil/imgutils.h>
	#include <libavdevice/avdevice.h>
	#include <libswscale/swscale.h>
}

#include <SDL.h>

#include <mutex>
#include <thread>
#include <string>
#include <chrono>

#include <string.h>
#include <shobjidl.h>
#include <Windows.h>
#include <wchar.h>
#include <tchar.h>
#include <conio.h>

#pragma warning(disable : 4819)
#pragma warning(disable : 4996)
#pragma warning(disable : 6031)
#pragma warning(disable : 26812)

#pragma comment(lib, "swscale.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avdevice.lib")
#pragma comment(lib, "avcodec.lib")

#undef main

#define IDM_FILE_OPEN 0
#define IDM_FILE_CLOSE 1

using namespace std::chrono;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK EnumWindowsProc(HWND, LPARAM);
HWND GetWallpaperWindow();

void RegisterMyWindow(HINSTANCE);
void drawLoop(const bool* check);
void clear();

int open(const char* file);
int wopen(const wchar_t* file);

void SetFile(const wchar_t* file);

const wchar_t* class_name = L"LiveDeskWallpaperGUI";
const wchar_t* title = L"Live Desktop Wallpaper GUI";

const AVPixelFormat dst_pix_fmt = AV_PIX_FMT_YUV420P;
int dst_width = 1920;
int dst_height = 1080;

int vstrm_idx = 0;

double framerate = 30.0f;
int framedist = 33;

AVFormatContext* inctx = nullptr;
SwsContext* swsctx = nullptr;
AVCodec* vcodec = nullptr;
AVStream* vstrm = nullptr;
AVFrame* frame = nullptr, * decframe = nullptr;
AVPacket packet = { 0, };

SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;
SDL_Texture* img = nullptr;

uint8_t* framebuf = nullptr;

HWND hWallpaper;

bool check = true;
std::thread wallpaper_thread;

int WinMain(
		HINSTANCE hInstance, 
		HINSTANCE hPrevInstance, 
		LPSTR lpCmdLine, 
		int nCmdShow) {
	HWND hWnd;
	MSG msg;
	
	SDL_Init(SDL_INIT_VIDEO);
	av_register_all();

	RegisterMyWindow(hInstance);

	hWnd = CreateWindow(class_name, title, WS_OVERLAPPEDWINDOW, 
		100, 90, 400, 350, NULL, NULL, hInstance, NULL);

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	hWallpaper = GetWallpaperWindow();

	window = SDL_CreateWindowFrom((void*)hWallpaper);
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

	SDL_GetWindowSize(window, &dst_width, &dst_height);

	img = SDL_CreateTexture(
		renderer,
		SDL_PIXELFORMAT_YV12,
		SDL_TEXTUREACCESS_STREAMING,
		dst_width,
		dst_height
	);

	if (lpCmdLine[0] != '\0') {
		check = false;

		open(lpCmdLine);
		wallpaper_thread = std::thread(drawLoop, &check);
	}

	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	check = true;
	wallpaper_thread.join();

	clear();
	
	SDL_DestroyTexture(img);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);

	SDL_Quit();

	ShowWindow(hWallpaper, SW_SHOW);

	return (int)msg.wParam;
}

void SetFile(const wchar_t* file_name) {
	if (check == false) {
		check = true;
		wallpaper_thread.join();

		clear();
	}

	wopen(file_name);
	wallpaper_thread = std::thread(drawLoop, &check);

	check = false;
}

int ShowFileOpenDialog(wchar_t* _dst) {
	IFileOpenDialog* pFileOpen;
	IShellItem* pItem;
	PWSTR pszFilePath;

	HRESULT hr = CoInitializeEx(NULL, 
		COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	
	if (SUCCEEDED(hr)) {
		hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
			IID_IFileOpenDialog, (void**)&pFileOpen);
		if (SUCCEEDED(hr)) {
			hr = pFileOpen->Show(NULL);
			if (SUCCEEDED(hr)) {
				hr = pFileOpen->GetResult(&pItem);
				if (SUCCEEDED(hr)) {
					hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
					if (SUCCEEDED(hr)) {
						lstrcpyW(_dst, pszFilePath);
						CoTaskMemFree(pszFilePath);
					}
					pItem->Release();
				}
			}
			pFileOpen->Release();
		}
		CoUninitialize();
	}

	return 0;
}

LRESULT CALLBACK WndProc(
		HWND hWnd, 
		UINT uMessage, 
		WPARAM wParam,
		LPARAM lParam) {
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

void RegisterMyWindow(HINSTANCE hInstance) {
	WNDCLASS wc = { 0, };

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

	RegisterClass(&wc);
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
	if (wallpaper_hwnd == nullptr) {
		exit(-1);
	}
	return wallpaper_hwnd;
}

int open(const char* file) {
	if (avformat_open_input(&inctx, file, nullptr, nullptr) < 0) {
		fprintf(stderr, "avformat_open_input failed!\n");
		return -1;
	}
	if (avformat_find_stream_info(inctx, nullptr)) {
		fprintf(stderr, "avformat_find_stream_info failed!\n");
		return -1;
	}

	vstrm_idx = av_find_best_stream(inctx, AVMEDIA_TYPE_VIDEO, -1, -1, &vcodec, 0);
	vstrm = inctx->streams[vstrm_idx];

	if (avcodec_open2(vstrm->codec, vcodec, nullptr) < 0) {
		fprintf(stderr, "avcodec_open2 failed!\n");
		return -1;
	}
	framerate = av_q2d(vstrm->codec->framerate);
	framedist = (int)(1000 / framerate);

	swsctx = sws_getContext(
		vstrm->codec->width, vstrm->codec->height, vstrm->codec->pix_fmt,
		dst_width, dst_height, dst_pix_fmt, SWS_BICUBIC, nullptr, nullptr, nullptr
	);

	frame = av_frame_alloc();
	decframe = av_frame_alloc();

	framebuf = new uint8_t[avpicture_get_size(dst_pix_fmt, dst_width, dst_height)];
	avpicture_fill((AVPicture*)frame, framebuf, dst_pix_fmt, dst_width, dst_height);

	return 0;
}

int wopen(const wchar_t* file) {
	int wide_len, multi_len, res;

	char* multibyte;

	wide_len = _tcslen(file);
	multi_len = WideCharToMultiByte(CP_ACP, 0, file, wide_len, nullptr, 0, nullptr, nullptr);

	multibyte = new char[multi_len + 1];
	WideCharToMultiByte(CP_ACP, 0, file, wide_len, multibyte, multi_len, nullptr, nullptr);

	res = open(multibyte);

	delete[] multibyte;

	return res;
}

void drawLoop(const bool* check) {
	system_clock::time_point start, end;
	milliseconds sec;

	int ret, got_pic;

	while(*check == false) {
		start = system_clock::now();

		ret = av_read_frame(inctx, &packet);
		if (ret == 0 && packet.stream_index != vstrm_idx) {
			av_free_packet(&packet);
			continue;
		}
		
		if (ret == AVERROR_EOF) {
			av_seek_frame(inctx, vstrm_idx, 0, AVSEEK_FLAG_FRAME);
			continue;
		}

		avcodec_decode_video2(vstrm->codec, decframe, &got_pic, &packet);
		if (!got_pic) {
			av_free_packet(&packet);
			continue;
		}

		sws_scale(swsctx, decframe->data, decframe->linesize, 0, decframe->height, frame->data, frame->linesize);
		SDL_UpdateYUVTexture(img, nullptr, 
			frame->data[0], frame->linesize[0], 
			frame->data[1], frame->linesize[1], 
			frame->data[2], frame->linesize[2]
		);

		SDL_RenderClear(renderer);
		SDL_RenderCopy(renderer, img, nullptr, nullptr);
		SDL_RenderPresent(renderer);

		end = system_clock::now();
		sec = duration_cast<milliseconds>(end - start);

		SDL_Delay((uint32_t)max(framedist - sec.count(), 0));

		av_free_packet(&packet);
	}
}

void clear() {
	if (framebuf != nullptr) {
		delete[] framebuf;
		framebuf = nullptr;
	}

	av_frame_free(&decframe);
	av_frame_free(&frame);

	avformat_close_input(&inctx);
}
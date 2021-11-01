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
#include <chrono>

#include <string.h>
#include <Windows.h>
#include <conio.h>

#pragma warning(disable : 4996)
#pragma warning(disable : 6031)
#pragma warning(disable : 26812)

#pragma comment(lib, "swscale.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avdevice.lib")
#pragma comment(lib, "avcodec.lib")

#undef main

using namespace std::chrono;

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lparam) {
	HWND p, *ret;

	p = FindWindowEx(hwnd, NULL, L"SHELLDLL_DefView", NULL);
	ret = (HWND*)lparam;

	if (p) *ret = FindWindowEx(NULL, hwnd, L"WorkerW", NULL);

	return true;
}

HWND get_wallpaper_window() {
	HWND progman, wallpaper_hwnd;

	progman = FindWindow(L"progman", NULL);
	wallpaper_hwnd = nullptr;

	SendMessageTimeout(progman, 0x052C, 0, 0, SMTO_NORMAL, 1000, nullptr);

	EnumWindows(EnumWindowsProc, (LPARAM)&wallpaper_hwnd);

	return wallpaper_hwnd;
}

void wallpaper_loop(const char* arg, const bool* check) {
	int vstrm_idx;
	int dst_width = 0, dst_height = 0;
	int ret, got_pic = 0;

	double framerate = 30.0f;
	int framedist = 33;

	AVFormatContext* inctx = nullptr;
	SwsContext* swsctx = nullptr;
	AVCodec* vcodec = nullptr;
	AVStream* vstrm = nullptr;
	AVFrame* frame = nullptr, * decframe = nullptr;
	
	AVPixelFormat dst_pix_fmt = AV_PIX_FMT_YUV420P;
	AVPacket packet;

	SDL_Window* window = nullptr;
	SDL_Renderer* renderer = nullptr;
	SDL_Texture* img = nullptr;

	uint8_t* framebuf;

	system_clock::time_point start, end;
	milliseconds sec;

	SDL_Init(SDL_INIT_VIDEO);
	
	window = SDL_CreateWindowFrom((void*)get_wallpaper_window());
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

	SDL_GetWindowSize(window, &dst_width, &dst_height);

	img = SDL_CreateTexture(
		renderer,
		SDL_PIXELFORMAT_YV12,
		SDL_TEXTUREACCESS_STREAMING,
		dst_width,
		dst_height
	);

	av_register_all();

	if (avformat_open_input(&inctx, arg, nullptr, nullptr) < 0) {
		fprintf(stderr, "avformat_open_input failed!\n");
		exit(-1);
	}
	if (avformat_find_stream_info(inctx, nullptr)) {
		fprintf(stderr, "avformat_find_stream_info failed!\n");
		exit(-1);
	}

	vstrm_idx = av_find_best_stream(inctx, AVMEDIA_TYPE_VIDEO, -1, -1, &vcodec, 0);
	vstrm = inctx->streams[vstrm_idx];

	if (avcodec_open2(vstrm->codec, vcodec, nullptr) < 0) {
		fprintf(stderr, "avcodec_open2 failed!\n");
		exit(-1);
	}
	framerate = av_q2d(vstrm->codec->framerate);
	framedist = (int)(1000 / framerate);

	printf("infile: %s\n", arg);
	printf("origin: %d x %d\n", vstrm->codec->width, vstrm->codec->height);
	printf("fps   : %lf\n", framerate);
	printf("output: %d x %d\n", dst_width, dst_height);

	swsctx = sws_getContext(
		vstrm->codec->width, vstrm->codec->height, vstrm->codec->pix_fmt,
		dst_width, dst_height, dst_pix_fmt, SWS_BICUBIC, nullptr, nullptr, nullptr
	);

	frame = av_frame_alloc();
	decframe = av_frame_alloc();

	framebuf = new uint8_t[avpicture_get_size(dst_pix_fmt, dst_width, dst_height)];
	avpicture_fill((AVPicture*)frame, framebuf, dst_pix_fmt, dst_width, dst_height);

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

		SDL_Delay(max(framedist - sec.count(), 0));

		av_free_packet(&packet);
	}

	SDL_Quit();
	delete[] framebuf;
}

int main(int argc, char* argv[]) {
	if (argc < 2) {
		fprintf(stderr, "this program need arguments!\n");
		exit(-1);
	}

	int command;
	bool check = false;
	HANDLE mutex;

	std::thread wallpaper_thread;

	mutex = OpenMutex(MUTEX_ALL_ACCESS, 0, L"LiveDeskWallpaper.exe");
	if (!mutex) {
		mutex = CreateMutex(0, 0, L"LiveDeskWallpaper.exe");
	}
	else {
		fprintf(stderr, "already has instance!\npress any key to close this window...\n");
		getch();
		exit(-1);
	}

	wallpaper_thread = std::thread(wallpaper_loop, argv[1], &check);
	while (!check) {
		command = getch();
		switch (command) {
		case 'Q':
		case 'q':
			check = true;
			wallpaper_thread.join();
			break;
		}
	}

	return 0;
}
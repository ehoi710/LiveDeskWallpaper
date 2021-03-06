#pragma once

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libavutil/avutil.h>
	#include <libavutil/imgutils.h>
	#include <libavdevice/avdevice.h>
	#include <libswscale/swscale.h>
}

#include <SDL.h>

#include <chrono>

#include <tchar.h>
#include <Windows.h>

#pragma comment(lib, "swscale.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avdevice.lib")
#pragma comment(lib, "avcodec.lib")

#pragma warning(disable : 4819)
#pragma warning(disable : 4996)
#pragma warning(disable : 6031)
#pragma warning(disable : 26812)

using namespace std::chrono;

class MediaProvider {
public:
	MediaProvider(HWND hWnd) {
		window = SDL_CreateWindowFrom(hWnd);

		SDL_GetWindowSize(window, &window_width, &window_height);

		renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
		texture = SDL_CreateTexture(
			renderer,
			//SDL_PIXELFORMAT_YV12,
			SDL_PIXELFORMAT_IYUV,
			SDL_TEXTUREACCESS_STREAMING,
			window_width, window_height
		);
	}
	~MediaProvider() {
		clear();

		SDL_DestroyTexture(texture);
		SDL_DestroyRenderer(renderer);
		SDL_DestroyWindow(window);
	}

	int open(const wchar_t* file_name) {
		int wide_len, multi_len, res;
		char* multibyte;

		wide_len = (int)wcslen(file_name);
		multi_len = WideCharToMultiByte(CP_ACP, 0, file_name, wide_len, nullptr, 0, nullptr, nullptr);

		multibyte = new char[multi_len + 1];
		multibyte[multi_len] = '\0'; // ???: 초기화는 조상님이 해줍니까?

		WideCharToMultiByte(CP_ACP, 0, file_name, wide_len, multibyte, multi_len, nullptr, nullptr);

		res = open(multibyte);

		delete[] multibyte;

		return res;
	}

	int open(const char* file_name) {
		int left, right;

		if (avformat_open_input(&inctx, file_name, nullptr, nullptr) < 0) {
			fprintf(stderr, "avformat_open_input failed!\n");
			throw std::exception("avformat_open_input failed!");
		}
		if (avformat_find_stream_info(inctx, nullptr)) {
			fprintf(stderr, "avformat_find_stream_info failed!\n");
			throw std::exception("avformat_find_stream_info failed!");
		}

		vstrm_idx = av_find_best_stream(inctx, AVMEDIA_TYPE_VIDEO, -1, -1, &vcodec, 0);
		vstrm = inctx->streams[vstrm_idx];

		if (avcodec_open2(vstrm->codec, vcodec, nullptr) < 0) {
			fprintf(stderr, "avcodec_open2 failed!\n");
			throw std::exception("avcodec_open2 failed!");
		}
		framerate = av_q2d(vstrm->codec->framerate);
		framedist = (int)(1000 / framerate);

		video_width = vstrm->codec->width;
		video_height = vstrm->codec->height;

		left = video_width * window_height;
		right = window_width * video_height;

		if (left == right) {
			draw_rect.w = window_width;
			draw_rect.h = window_height;

			draw_rect.x = 0;
			draw_rect.y = 0;
		}
		else if (left > right) {
			// 윈도우의 높이가 너무 높다!
			draw_rect.w = window_width;
			draw_rect.h = (video_height * window_width) / video_width;

			draw_rect.x = 0;
			draw_rect.y = (window_height - draw_rect.h) / 2;
		}
		else if (left < right) {
			// 윈도우의 너비가 너무 넓다!
			draw_rect.w = (video_width * window_height) / video_height;
			draw_rect.h = window_height;

			draw_rect.x = (window_width - draw_rect.w) / 2;
			draw_rect.y = 0;
		}

		swsctx = sws_getContext(
			video_width, video_height, vstrm->codec->pix_fmt,
			draw_rect.w, draw_rect.h, dst_pix_fmt, SWS_BICUBIC, nullptr, nullptr, nullptr
		);

		frame = av_frame_alloc();
		decframe = av_frame_alloc();

		framebuf = new uint8_t[avpicture_get_size(dst_pix_fmt, draw_rect.w, draw_rect.h)];
		avpicture_fill((AVPicture*)frame, framebuf, dst_pix_fmt, draw_rect.w, draw_rect.h);

		isOpened = true;
		return 0;
	}

	void drawLoop(const bool* check) {
		if (this == nullptr) return;
		if (!isOpened) return;

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
			SDL_UpdateYUVTexture(texture, &draw_rect,
			 	frame->data[0], frame->linesize[0], 
			 	frame->data[1], frame->linesize[1], 
			 	frame->data[2], frame->linesize[2]
			);

			// SDL_UpdateTexture(texture, &draw_rect, framebuf, video_height);
			
			SDL_RenderClear(renderer);
			SDL_RenderCopy(renderer, texture, &draw_rect, &draw_rect);
			SDL_RenderPresent(renderer);

			end = system_clock::now();
			sec = duration_cast<milliseconds>(end - start);

			SDL_Delay((uint32_t)max(framedist - sec.count(), 0));

			av_free_packet(&packet);
		}
	}

	void fetchOne() {
		int ret, got_pic, cnt = 2;

		if (!isOpened) {
			SDL_RenderClear(renderer);
			SDL_RenderPresent(renderer);

			return;
		}

		while (cnt--) {
			av_seek_frame(inctx, vstrm_idx, 0, AVSEEK_FLAG_FRAME);

			ret = av_read_frame(inctx, &packet);
			if (ret == 0 && packet.stream_index != vstrm_idx) {
				av_free_packet(&packet);
				continue;
			}

			avcodec_decode_video2(vstrm->codec, decframe, &got_pic, &packet);
			if (!got_pic) {
				av_free_packet(&packet);
				continue;
			}

			sws_scale(swsctx, decframe->data, decframe->linesize, 0,
				decframe->height, frame->data, frame->linesize);

			SDL_UpdateYUVTexture(texture, &draw_rect,
				frame->data[0], frame->linesize[0],
				frame->data[1], frame->linesize[1],
				frame->data[2], frame->linesize[2]
			);

			SDL_RenderClear(renderer);
			SDL_RenderCopy(renderer, texture, &draw_rect, &draw_rect);
			SDL_RenderPresent(renderer);

			av_free_packet(&packet);
		}
		return;
	}

	void clear() {
		if (framebuf != nullptr) {
			delete[] framebuf;
			framebuf = nullptr;
		}

		av_frame_free(&decframe);
		av_frame_free(&frame);

		avformat_close_input(&inctx);

		isOpened = false;
	}

private:
	AVFormatContext* inctx = nullptr;
	SwsContext* swsctx = nullptr;
	AVCodec* vcodec = nullptr;
	AVStream* vstrm = nullptr;
	AVFrame* frame = nullptr, * decframe = nullptr;
	AVPacket packet = { 0, };

	const AVPixelFormat dst_pix_fmt = AV_PIX_FMT_YUV420P;

	double framerate = 30.0f;
	int framedist = 33;

	uint8_t* framebuf = nullptr;

	int vstrm_idx = -1;

	int window_width = 0, window_height = 0;
	int video_width = 0, video_height = 0;

	SDL_Rect draw_rect;

	SDL_Window* window = nullptr;
	SDL_Renderer* renderer = nullptr;
	SDL_Texture* texture = nullptr;

	bool isOpened = false;
};
//
//  main.cpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/6/25.
//

//#include <iostream>
//
//int main(int argc, const char * argv[]) {
//    // insert code here...
//    std::cout << "Hello, World!\n";
//    return 0;
//}
//
//#include "player.h"
//
//int main(int argc, char *argv[])
//{
//    player_running("/Volumes/external_disk/code/GaiaPlayer/1.mp4");
//
//    return 0;
//}

#include <atomic>
#include <string>
#include <iostream>
 
#ifdef __cplusplus
extern "C" {
#endif
 
#include <libavutil/avutil.h>
#include <libavdevice/avdevice.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <SDL2/SDL.h>
#include <libavutil/parseutils.h>
#include <libavutil/bprint.h>
#include <libavutil/tree.h>
#include <libavutil/eval.h>
#include <libavutil/lfg.h>
#include <libavutil/timecode.h>
#include <libavutil/file.h>
#include <libavutil/random_seed.h>
#include <fenv.h>
 
#ifdef __cplusplus
}
#endif
 
#define SFM_REFRESH_EVENT (SDL_USEREVENT + 1)
#define SFM_BREAK_EVENT (SDL_USEREVENT + 2)
 
using namespace std;
 
atomic<bool> stopThread(false);
atomic<bool> stopRefresh(false);
 
int refreshThread (void *opaque) {
    stopThread = false;
    SDL_Event event;
    event.type = SFM_REFRESH_EVENT;
    while (!stopThread) {
        if (!stopRefresh) {
            SDL_PushEvent(&event);
        }
        SDL_Delay(40); // 40ms 刷新一次SDL
    }
    stopThread = false;
    stopRefresh = false;
 
    {
        SDL_Event event;
        event.type = SFM_BREAK_EVENT;
        SDL_PushEvent(&event);
    }
 
    return 0;
}
 
#include <folly/experimental/coro/Task.h>
#include <folly/experimental/coro/BlockingWait.h>
#include <folly/futures/Future.h>
#include <folly/executors/GlobalExecutor.h>
#include <folly/init/Init.h>
#include <iostream>

folly::coro::Task<int> slow() {
  std::cout << "before sleep" << std::endl;
  co_await folly::futures::sleep(std::chrono::seconds{1});
  std::cout << "after sleep" << std::endl;
  co_return 1;
}

int main () {
    
    folly::coro::blockingWait(
          slow().scheduleOn(folly::getGlobalCPUExecutor().get()));
    
    string filepath = "/Volumes/external_disk/code/GaiaPlayer/1.mp4";
 
    avdevice_register_all();
    avformat_network_init();
 
    AVFormatContext *avFormatContext = avformat_alloc_context();
    if (avformat_open_input(&avFormatContext, filepath.c_str(), NULL, NULL) != 0) { // 打开输入流
        cerr << "Failed to open input stream" << endl;
        exit(1);
    }
 
    if (avformat_find_stream_info(avFormatContext, NULL) < 0) { // 分析流信息
        cerr << "Failed to find stream information." << endl;
        exit(1);
    }
 
 
    int videoIndex = -1;
    for (int i=0;i<avFormatContext->nb_streams;i++) { // 查找第一个视频流的索引
        if (avFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoIndex = i;
            break;
        }
    }
 
    if (videoIndex == -1) {
        cerr << "Failed to find a video stream." << endl;
        exit(1);
    }
 
    AVStream *avStream = avFormatContext->streams[videoIndex];
    const AVCodec *avCodec = avcodec_find_decoder(avStream->codecpar->codec_id);
 
    AVCodecContext *avCodecContext = avcodec_alloc_context3(avCodec); // 为该解码器新建解码上下文
    if (!avCodecContext) {
        cerr << "Failed to allocate the decoder context for stream" << endl;
        return -1;
    }
    if(avcodec_parameters_to_context(avCodecContext, avStream->codecpar) < 0) { // 将流的解码参数拷贝到解码上下文
        cerr << "Failed to copy decoder parameters to input decoder context." << endl;
        return -1;
    }
 
    avCodecContext->pkt_timebase = avStream->time_base;
    cout << avCodec->id << endl;
 
    if (avcodec_open2(avCodecContext, avCodec, NULL) < 0) {
        cerr << "Failed to open codec." << endl;
        exit(1);
    }
 
    // yuvFrame用于存储新帧，这段代码是为其分配存储空间
    AVFrame *yuvFrame = av_frame_alloc();
    int bufferSize = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, avCodecContext->width, avCodecContext->height, 1);
    unsigned char *buffer = (unsigned char*)av_malloc(bufferSize);
    av_image_fill_arrays(yuvFrame->data, yuvFrame->linesize, buffer, AV_PIX_FMT_YUV420P, avCodecContext->width, avCodecContext->height, 1);
 
    //Output Info-----------------------------
    printf("---------------- File Information ---------------\n");
    av_dump_format(avFormatContext, 0, filepath.c_str(),0);
    printf("-------------------------------------------------\n");
 
    SwsContext *imgConvertContext = sws_getContext(avCodecContext->width, avCodecContext->height, avCodecContext->pix_fmt,
            avCodecContext->width, avCodecContext->height, AV_PIX_FMT_YUV420P,
            SWS_BICUBIC, NULL, NULL, NULL);
 
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        cerr << "Failed to initialization SDL - " <<  SDL_GetError();
        exit(1);
    }
 
    int screenW = avCodecContext->width;
    int screenH = avCodecContext->height;
    SDL_Window *screen = SDL_CreateWindow("simple ffmpeg player's window", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                          screenW, screenH, SDL_WINDOW_OPENGL);
 
    if (!screen) {
        cerr << "SDL: could not create window - " << SDL_GetError();
        exit(1);
    }
 
    SDL_Renderer *sdlRenderer = SDL_CreateRenderer(screen, -1, 0);
 
    SDL_Texture *sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, avCodecContext->width, avCodecContext->height);
 
    SDL_Rect sdlRect;
    sdlRect.x = 0;
    sdlRect.y = 0;
    sdlRect.w = screenW;
    sdlRect.h = screenH;
 
    SDL_CreateThread(refreshThread, NULL, NULL);
 
    SDL_Event event;
    AVPacket *avPacket = (AVPacket*) av_malloc(sizeof(AVPacket));
    AVFrame *frame = av_frame_alloc();
 
    double timeBase = av_q2d(avStream->time_base);
    double frameRate = av_q2d(avStream->avg_frame_rate);
    double interval = 1 / (timeBase * frameRate);
 
    while (av_read_frame(avFormatContext, avPacket) >= 0) { // 从format上下文中读取压缩编码的数据包
        if (avPacket->stream_index == videoIndex) { // 只处理视频类型的packet，音频类型的packet
            if (avcodec_send_packet(avCodecContext, avPacket) < 0) { // 将packet提交到解码上下文
                cerr << "Failed to submitting the packet to the decoder\n";
                return -1;
            }
            int ret;
            while (true) {
                SDL_WaitEvent(&event);// 阻塞等待事件
 
                if (event.type == SFM_REFRESH_EVENT) {
                    if (avcodec_receive_frame(avCodecContext, frame) >= 0) { // 从解码上下文中读取帧frame
 
                        sws_scale(imgConvertContext, frame->data, frame->linesize, 0, avCodecContext->height, yuvFrame->data, yuvFrame->linesize);
                        yuvFrame->height = frame->height;
                        yuvFrame->width = frame->width;
 
                        SDL_UpdateTexture(sdlTexture, NULL, yuvFrame->data[0], yuvFrame->linesize[0]);
                        SDL_RenderClear(sdlRenderer);
                        SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
                        SDL_RenderPresent(sdlRenderer);
                        av_frame_unref(frame);
                    } else {
                        break;
                    }
 
                } else if(event.type==SDL_KEYDOWN){
                    //Pause
                    if(event.key.keysym.sym==SDLK_SPACE)
                        stopRefresh =! stopRefresh;
                }else if(event.type==SDL_QUIT){
                    stopThread = 1;
                }else if(event.type==SFM_BREAK_EVENT){
                    break;
                }
            }
        }
        av_packet_unref(avPacket);
    }
 
    sws_freeContext(imgConvertContext);
 
    SDL_Quit();
    av_frame_unref(frame);
    av_frame_unref(yuvFrame);
    avcodec_close(avCodecContext);
    avformat_close_input(&avFormatContext);
    return 0;
}

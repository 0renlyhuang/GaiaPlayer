//
//  engine_env.hpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/7/9
//  
//
    

#ifndef engine_env_hpp
#define engine_env_hpp

#include "base/time/time_unit.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/frame.h>
#include <libavutil/time.h>
#include <libavutil/imgutils.h>
}

#include <memory>
#include <chrono>

#include <folly/observer/SimpleObservable.h>

namespace gaia::engine {

struct Streams {
    AVStream *a_stream;
    AVStream *v_stream;
    int a_idx;
    int v_idx;
};

struct PacketInfo {
    enum class PktType {
        kVideo,
        kAudio,
        kSubtitle,
    };
    
    AVPacket *pkt;
    PktType pkt_type;
};


struct EngineEnv {
    EngineEnv() {}
    std::string file = "";
    AVFormatContext *ctx = nullptr;
    std::shared_ptr<Streams> streams = nullptr;
    int32_t serial = 0;
    base::TimeUnit max_frame_duration;
    AVRational guess_frame_rate;
    base::TimeUnit guess_frame_duration;
};

}

#endif /* engine_env_hpp */

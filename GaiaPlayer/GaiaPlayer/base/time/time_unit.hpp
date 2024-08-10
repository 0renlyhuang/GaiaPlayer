//
//  time_unit.hpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/8/7
//  
//
    

#ifndef time_unit_hpp
#define time_unit_hpp

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/frame.h>
#include <libavutil/time.h>
#include <libavutil/imgutils.h>
}

#include <chrono>
#include <format>



namespace gaia::base {

using TimeUnit = std::chrono::microseconds;
//constexpr auto microseconds_per_sec = 1000000;  AV_TIME_BASE

static TimeUnit toTimeUnit(int64_t pts, AVRational src_timebase) {
    const auto microsec = av_rescale_q(pts, src_timebase, AV_TIME_BASE_Q);
    return TimeUnit(microsec);
}

static std::string toSecStr(base::TimeUnit ts) {
    return std::format("{}", std::chrono::duration_cast<std::chrono::duration<double>>(ts));
}

}

#endif /* time_unit_hpp */

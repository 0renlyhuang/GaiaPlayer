//
//  clock.cpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/8/4
//  
//
    

#include "clock.hpp"

#include "base/log/log.hpp"

extern "C" {
#include <libavutil/time.h>
}


namespace gaia::engine {

Clock::Clock(MasterType master_type, base::TimeUnit start_pts): master_type_(master_type), ts_(start_pts), pts_src_(start_pts), ts_ob_(this->pts_src_.getObserver()) {
    
}

void Clock::onAudioClockPass(base::TimeUnit uint) {
    if (master_type_ == MasterType::kAudio) {
        this->ts_ += uint;
        this->pts_src_.setValue(this->ts_);
        this->ts_src_.setValue(this->ts_);
    }
    
    
    this->audio_now_ += uint;
    const auto ab_ts = base::toTimeUnit(av_gettime_relative(), AV_TIME_BASE_Q);
    this->absolute_now_ = ab_ts - this->absolute_start_ts_;
    XLOG(DBG, std::format("clock, audio update, slow:{}", std::chrono::duration_cast<std::chrono::duration<double>>(this->absolute_now_ - this->audio_now_)));
}

void Clock::updateVideoClock(base::TimeUnit ts) {
    this->video_now_ = ts;
    const auto ab_ts = base::toTimeUnit(av_gettime_relative(), AV_TIME_BASE_Q);
    this->absolute_now_ = ab_ts - this->absolute_start_ts_;
    XLOG(DBG, std::format("clock, video update, slow:{}", std::chrono::duration_cast<std::chrono::duration<double>>(this->absolute_now_ - this->video_now_)));
}

void Clock::setNow(base::TimeUnit ts) {
    this->ts_ = ts;
    this->pts_src_.setValue(this->ts_);
    
    
    this->absolute_start_ts_ = base::toTimeUnit(av_gettime_relative(), AV_TIME_BASE_Q);
    this->absolute_now_ = ts;
    this->audio_now_ = ts;
    this->video_now_ = ts;
}

base::TimeUnit Clock::now() const {
    return this->ts_;
}

base::TimeUnit Clock::absoluteNow() {
    return std::chrono::duration_cast<base::TimeUnit>(std::chrono::microseconds(av_gettime_relative()));
}

folly::observer::Observer<base::TimeUnit>& Clock::tsObserver() {
    return this->ts_ob_;
}

}

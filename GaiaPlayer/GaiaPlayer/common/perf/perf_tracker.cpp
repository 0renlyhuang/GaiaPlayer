//
//  perf_tracker.cpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/8/12
//  
//
    

#include "perf_tracker.hpp"

#include "base/log/log.hpp"

namespace gaia::engine {

PerfTracker::PerfTracker(std::shared_ptr<Clock> clock, std::shared_ptr<EngineEnv> env): clock_(clock), env_(env) {
    
}

void PerfTracker::trackPictureShow() {
    const auto now = clock_->absoluteNow();
    ++this->pic_cnt_;
    
    if (!this->first_pic_shown_ts_.has_value()) {
        this->first_pic_shown_ts_ = now;
        this->last_pic_shown_ts_ = now;
        return;
    }
    
    
    const auto exptected_frame_per_sec = this->env_->guess_frame_rate.num / this->env_->guess_frame_rate.den;
    
    const auto curr_frame_per_sec = std::chrono::seconds(1) / base::TimeUnit(1) / (now - last_pic_shown_ts_.value()).count();
    const auto curr_complted_rate = 1.0 * curr_frame_per_sec / exptected_frame_per_sec;
    
    const auto average_frame_per_sec = std::chrono::seconds(1) / base::TimeUnit(1) * pic_cnt_ / (now - first_pic_shown_ts_.value()).count();
    const auto average_completed_rate = 1.0 * average_frame_per_sec / exptected_frame_per_sec;
    
    XLOG(DBG0, std::format("[Perf] v: [c]{}fps,{:.1f}%, [av]{}fps,{:.1f}%, [e]{}fps", static_cast<int>(curr_frame_per_sec), curr_complted_rate * 100, static_cast<int>(average_frame_per_sec), average_completed_rate * 100, exptected_frame_per_sec));
    
    this->last_pic_shown_ts_ = now;
}

void PerfTracker::trackAudioSamplePlay() {
    
}

void PerfTracker::trackDropVideoFrame(DecodedFramePtr frame) {
    ++drop_v_frame_cnt_;
    
    using namespace std::chrono;
    const auto pts = duration_cast<duration<double>>(frame->pts.value_or(base::TimeUnit(-1)));
    const auto drop_rate = 1.0 * drop_v_frame_cnt_ / (show_v_frame_cnt + drop_v_frame_cnt_);
    XLOG(DBG0, std::format("[Perf] df, v:{}, rate:{:.2f}%, cnt:{}", pts, drop_rate * 100, drop_v_frame_cnt_));
}

void PerfTracker::trackShowVideoFrame(DecodedFramePtr frame) {
    ++show_v_frame_cnt;
}


void PerfTracker::trackStartProduce(std::string msg) {
    XLOG(DBG0, std::format("[produce] start, {}", std::move(msg)));
}

void PerfTracker::trackPauseProduce(std::string msg) {
    XLOG(DBG0, std::format("[produce] pause, {}", std::move(msg)));
}


}

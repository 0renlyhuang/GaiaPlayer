//
//  sdl_video_consumer.cpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/8/6
//  
//
    

#include "sdl_video_consumer.hpp"

#include "base/log/log.hpp"

#include <folly/experimental/coro/Sleep.h>
#include <folly/experimental/coro/BlockingWait.h>

#include <format>



namespace gaia::engine {


constexpr auto drop_frame_threshold = base::TimeUnit(std::chrono::milliseconds(100));

SDLVideoConsumer::SDLVideoConsumer(std::shared_ptr<EngineEnv> env, std::shared_ptr<ConsumeDataSource> consume_data_source, std::shared_ptr<EventCenter> event_center, std::shared_ptr<base::Executors> executor, std::shared_ptr<Clock> clock, std::shared_ptr<SDLDst> sdl_dst, std::shared_ptr<PerfTracker> perf_tracker, std::shared_ptr<Demuxer> demuxer)
: env_(env), data_source_(consume_data_source), event_center_(event_center), executor_(executor), clock_(clock), sdl_dst_(sdl_dst), perf_tracker_(perf_tracker), demuxer_(demuxer) {
    
    const auto produce_subscription = this->event_center_->video_frame_produce_src.subscribe([this](const auto snap){
        this->executor_->Logic()->add([this]{
            this->onVideoFrameProduced();
        });
    });
    this->subscriptions_.push_back(produce_subscription);
    
    const auto clock_subscription = this->clock_->ts_src_.subscribe([this](const auto snap){
        this->executor_->Logic()->add([this]{
            this->onClockTick();
        });
    });
    this->subscriptions_.push_back(clock_subscription);
    
}

SDLVideoConsumer::~SDLVideoConsumer() {
    
}

void SDLVideoConsumer::onVideoFrameProduced() {
    XLOG(DBG) << "onVideoFrameProduced";
    this->checkFrame();
}

void SDLVideoConsumer::onClockTick() {
//    XLOG(DBG) << "onClockTick";
    this->checkFrame();
}

void SDLVideoConsumer::checkFrame() {
    this->tryRefreshFrame();
    this->demuxer_->resumeDemuxIfNeeded(this->env_->serial);
}

void SDLVideoConsumer::tryRefreshFrame() {
    const auto remain_duration_opt = this->refreshFrame();
    XLOG(DBG, std::format("refreshFrame remain_duration:{}", remain_duration_opt.value_or(base::TimeUnit(-1))));
    if (!remain_duration_opt.has_value()) {
        return;
    }
    
    const auto remain_duration = remain_duration_opt.value();
    if (remain_duration == base::TimeUnit(0)) {
        this->checkFrame();
        return;
    }
    
    const auto next_refresh_ts = Clock::absoluteNow() + remain_duration;
    if (this->next_refresh_ts_.has_value()) {
        if (this->next_refresh_ts_.value() <= next_refresh_ts) {  // has early task
            return;
        }
    }

    this->fresh_task_cancel_source_.requestCancellation();
    this->fresh_task_cancel_source_ = {};
    this->next_refresh_ts_ = next_refresh_ts;
    
    const auto when_finished = folly::coro::co_withCancellation(this->fresh_task_cancel_source_.getToken(), [remain_duration, this]() -> folly::coro::Task<void> {
        co_await folly::coro::sleep(remain_duration);
        
        this->checkFrame();
    })()
        .scheduleOn(this->executor_->Logic().get())
        .start();
}



std::optional<base::TimeUnit> SDLVideoConsumer::refreshFrame() {
    auto &queue_ref = this->data_source_->getVideoQueueRef();
    
    if (queue_ref.empty()) {
        return std::nullopt;
    }
    
    auto already_display_duration = base::TimeUnit(0);
    if (this->curr_frame_display_ts_.has_value()) {
        const auto curr_ts = Clock::absoluteNow();
        already_display_duration = std::max(curr_ts - this->curr_frame_display_ts_.value(), base::TimeUnit(0));
    }
     
    
    const auto master_ts = this->clock_->now();
    bool use_next_frame = false;
    if (!this->curr_decoded_frame_.has_value()) {
        use_next_frame = true;
    }
    else if (queue_ref.front()->pts <= master_ts) {
        use_next_frame = true;
    }
    else if (already_display_duration >= this->curr_decoded_frame_.value()->duration) {
        use_next_frame = true;
    }
    
    DecodedFramePtr frame;
    if (use_next_frame) {
        frame = queue_ref.front();
        queue_ref.pop();
        XLOG(DBG) << "pop frame";
    }
    else {
        frame = this->curr_decoded_frame_.value();
    }
    
    
    const auto video_ts = frame->pts;
    const auto sync_strategy = this->getVideoFrameStrategy(video_ts, master_ts);
    XLOGF(DBG, std::format("sync_strategy {}", static_cast<int>(sync_strategy)));
    const auto synced_frame = this->applySyncStrategy(frame, master_ts, sync_strategy);
    
    if (this->curr_decoded_frame_.has_value() && synced_frame == this->curr_decoded_frame_.value()) {
        if (already_display_duration >= synced_frame->duration) {
            return base::TimeUnit(0);
        }
        
        return synced_frame->duration - already_display_duration;
    }
    
    
    this->clock_->updateVideoClock(synced_frame->pts.value());
    this->sdl_dst_->showFrame(synced_frame);
    this->curr_decoded_frame_ = synced_frame;
    this->curr_frame_display_ts_ = Clock::absoluteNow();
    this->perf_tracker_->trackShowVideoFrame(synced_frame);
    
    return synced_frame->duration;
}


DecodedFramePtr SDLVideoConsumer::applySyncStrategy(DecodedFramePtr a_frame, base::TimeUnit master_ts, SyncStrategy sync_strategy) {
    DecodedFramePtr frame = a_frame;
    switch (sync_strategy) {
        case SyncStrategy::kDropFrame: {
            this->perf_tracker_->trackDropVideoFrame(frame);
            
            auto &queue_ref = this->data_source_->getVideoQueueRef();
            while (!queue_ref.empty()) {
                frame = queue_ref.front();
                queue_ref.pop();
                
                if (frame->pts >= master_ts - drop_frame_threshold) {
                    break;
                }
                else {
                    this->perf_tracker_->trackDropVideoFrame(frame);
                }
            }
            break;
        }
        case SyncStrategy::kSpeedUp: {
            if (frame->pts.has_value()) {
                const auto slow = master_ts - frame->pts.value();
                if (frame->duration > slow) {
                    frame->duration -= slow;
                    break;
                }
            }
            frame->duration /= 10;
            break;
        }
        case SyncStrategy::kSlowDown: {
            frame->duration *= 2;
            break;
        }
        case SyncStrategy::kMaxSlowDown: {
            frame->duration *= 5;
            break;
        }
            
        default:
            break;
    }
    
    return frame;
}

SDLVideoConsumer::SyncStrategy SDLVideoConsumer::getVideoFrameStrategy(std::optional<base::TimeUnit> video_ts_opt, base::TimeUnit master_ts) {
    using namespace std::chrono_literals;
    constexpr auto sync_threshold = std::chrono::duration_cast<base::TimeUnit>(5ms);
    constexpr auto max_sync_threshold = std::chrono::duration_cast<base::TimeUnit>(3s);
    
    if (!video_ts_opt.has_value()) {
        return SyncStrategy::kNormal;
    }
    
    const auto video_ts = video_ts_opt.value();
    
    if (master_ts <= sync_threshold)
        return SyncStrategy::kNormal;
    
    if (master_ts > drop_frame_threshold) {
        if (video_ts <= master_ts - drop_frame_threshold) {
            const auto& last_frame = this->data_source_->getVideoQueueRef().back();
            
            if (last_frame->pts.has_value() && last_frame->pts.value() + drop_frame_threshold > master_ts)
                return SyncStrategy::kDropFrame;
            
            return SyncStrategy::kSpeedUp;
        }
    }
    
    
    if (video_ts <= master_ts - sync_threshold) {
        return SyncStrategy::kSpeedUp;
    }
    
    else if (video_ts > master_ts - sync_threshold && video_ts < master_ts + sync_threshold) {
        return SyncStrategy::kNormal;
    }
    
    else if (video_ts >= master_ts + sync_threshold && video_ts < master_ts + max_sync_threshold) {
        return SyncStrategy::kSlowDown;
    }
    else if (video_ts >= master_ts + max_sync_threshold) {
        return SyncStrategy::kMaxSlowDown;
    }
    
    return SyncStrategy::kNormal;
}


}

//
//  sdl_video_consumer.hpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/8/6
//  
//
    

#ifndef sdl_video_consumer_hpp
#define sdl_video_consumer_hpp

#include "engine/engine_env.hpp"
#include "common/event/event_center.hpp"
#include "engine/consume/consume_data_source.hpp"
#include "base/async/executors.hpp"
#include "base/time/time_unit.hpp"
#include "engine/control/clock.hpp"
#include "engine/destination/sdl_dst.hpp"
#include "common/perf/perf_tracker.hpp"
#include "engine/demux/demuxer.hpp"
#include "base/observable/observable.hpp"

#include <folly/experimental/coro/Task.h>

#include <memory>
#include <chrono>

namespace gaia::engine {

class SDLVideoConsumer {
public:
    SDLVideoConsumer(std::shared_ptr<EngineEnv> env, std::shared_ptr<ConsumeDataSource> consume_data_source, std::shared_ptr<EventCenter> event_center, std::shared_ptr<base::Executors> executor, std::shared_ptr<Clock> clock, std::shared_ptr<SDLDst> sdl_dst, std::shared_ptr<PerfTracker> perf_tracker, std::shared_ptr<Demuxer> demuxer);
    ~SDLVideoConsumer();
    
private:
    void onVideoFrameProduced();
    void onClockTick();
    
    void checkFrame();
    void tryRefreshFrame();
    std::optional<base::TimeUnit> refreshFrame();
    
    
    
    enum class SyncStrategy: int {
        kMaxSlowDown = 1,
        kSlowDown,
        kNormal,
        kSpeedUp,
        kDropFrame,
    };
    SyncStrategy getVideoFrameStrategy(std::optional<base::TimeUnit> video_ts_opt, base::TimeUnit master_ts);
    DecodedFramePtr applySyncStrategy(DecodedFramePtr a_frame, base::TimeUnit master_ts, SyncStrategy sync_strategy);
    
    std::shared_ptr<EngineEnv> env_;
    std::shared_ptr<ConsumeDataSource> data_source_;
    std::shared_ptr<EventCenter> event_center_;
    std::shared_ptr<base::Executors> executor_;
    std::shared_ptr<Clock> clock_;
    std::shared_ptr<SDLDst> sdl_dst_;
    std::shared_ptr<PerfTracker> perf_tracker_;
    std::shared_ptr<Demuxer> demuxer_;
    
    std::vector<base::SubscribeHandlerPtr> subscriptions_;
    
    std::optional<DecodedFramePtr> curr_decoded_frame_;
    std::optional<base::TimeUnit> curr_frame_display_ts_;
    
    std::optional<base::TimeUnit> next_refresh_ts_;
    folly::CancellationSource fresh_task_cancel_source_;
    std::optional<folly::coro::Task<void>> last_refresh_task_;
    
    
};

}

#endif /* sdl_video_consumer_hpp */

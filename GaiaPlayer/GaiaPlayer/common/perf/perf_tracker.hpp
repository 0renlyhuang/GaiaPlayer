//
//  perf_tracker.hpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/8/12
//  
//
    

#ifndef perf_tracker_hpp
#define perf_tracker_hpp

#include "base/time/time_unit.hpp"
#include "engine/control/clock.hpp"
#include "engine/decode/decoded_content.hpp"
#include "engine/engine_env.hpp"

#include <folly/Utility.h>

#include <optional>

namespace gaia::engine {

class PerfTracker: public folly::MoveOnly {
public:
    PerfTracker(std::shared_ptr<Clock> clock, std::shared_ptr<EngineEnv> env);
    
    void trackPictureShow();
    void trackAudioSamplePlay();
    void trackDropVideoFrame(DecodedFramePtr frame);
    void trackShowVideoFrame(DecodedFramePtr frame);
    
    void trackStartProduce(std::string msg);
    void trackPauseProduce(std::string msg);
    
private:
    std::optional<base::TimeUnit> last_pic_shown_ts_;
    std::optional<base::TimeUnit> first_pic_shown_ts_;
    uint32_t pic_cnt_ = 0;
    
    std::optional<base::TimeUnit> last_audio_sample_play_;
    
    uint32_t drop_v_frame_cnt_ = 0;
    uint32_t show_v_frame_cnt = 0;
    
    std::shared_ptr<Clock> clock_;
    std::shared_ptr<EngineEnv> env_;
};

}

#endif /* perf_tracker_hpp */

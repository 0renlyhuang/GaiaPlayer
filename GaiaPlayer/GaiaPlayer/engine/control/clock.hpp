//
//  clock.hpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/8/4
//  
//
    

#ifndef clock_hpp
#define clock_hpp

#include "base/time/time_unit.hpp"

#include <folly/observer/SimpleObservable.h>

#include <chrono>

namespace gaia::engine {

class Clock {

public:
    enum class MasterType {
        kAudio,
    };
    
    
    Clock(MasterType master_type, base::TimeUnit start_pts);
    
    void onAudioClockPass(base::TimeUnit uint);
    void setNow(base::TimeUnit ts);
    
    folly::observer::Observer<base::TimeUnit>& tsObserver();
    
    base::TimeUnit now() const;
    static base::TimeUnit absoluteNow();
    
    // debug
    void updateVideoClock(base::TimeUnit ts);
    
private:
    MasterType master_type_;
    base::TimeUnit ts_;
    folly::observer::SimpleObservable<base::TimeUnit> pts_src_;
    folly::observer::Observer<base::TimeUnit> ts_ob_;
    
    
    
    base::TimeUnit absolute_start_ts_;
    base::TimeUnit absolute_now_;
    base::TimeUnit audio_now_;
    base::TimeUnit video_now_;
    
};

}

#endif /* clock_hpp */

//
//  event_center.hpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/8/6
//  
//
    

#ifndef event_center_hpp
#define event_center_hpp

#include <folly/observer/SimpleObservable.h>

namespace gaia::engine {

class EventCenter {
public:
    EventCenter();
    
    folly::observer::SimpleObservable<folly::Unit> video_frame_produce_notify;
    folly::observer::Observer<folly::Unit> video_frame_produce_observer;
};

}

#endif /* event_center_hpp */

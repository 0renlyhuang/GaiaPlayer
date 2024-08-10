//
//  event_center.cpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/8/6
//  
//
    

#include "event_center.hpp"

namespace gaia::engine {


EventCenter::EventCenter() :video_frame_produce_observer(this->video_frame_produce_notify.getObserver()) {

}



}

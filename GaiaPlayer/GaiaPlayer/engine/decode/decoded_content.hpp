//
//  decoded_content.hpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/7/31
//  
//
    

#ifndef decoded_content_hpp
#define decoded_content_hpp

#include "common/packet.hpp"
#include "common/frame.hpp"
#include "base/time/time_unit.hpp"

#include <chrono>
#include <optional>

namespace gaia::engine {

struct DecodedPacket {
    PacketPtr pkt;
    
};


struct DecodedFrame {
    FramePtr frame;
    PacketPtr source_pkt;
    
    std::optional<base::TimeUnit> pts;
    int64_t pos;
    int serial;
    base::TimeUnit duration;
};

using DecodedFramePtr = std::shared_ptr<DecodedFrame>;

}

#endif /* decoded_content_hpp */

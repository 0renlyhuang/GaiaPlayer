//
//  packet.hpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/7/31
//  
//
    

#ifndef packet_hpp
#define packet_hpp

extern "C" {
#include <libavcodec/packet.h>
}

#include <memory>

namespace gaia::engine {

struct Packet {
    AVPacket *raw;
    
    Packet() {
        this->raw = av_packet_alloc();
    }
    
    ~Packet() {
        av_packet_free(&raw);
    }
};

using PacketPtr = std::shared_ptr<Packet>;


static PacketPtr makePacket() {
    return std::make_shared<Packet>();
}

}

#endif /* packet_hpp */

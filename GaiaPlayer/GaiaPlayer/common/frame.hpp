//
//  frame.hpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/7/31
//  
//
    

#ifndef frame_hpp
#define frame_hpp

extern "C" {
#include <libavutil/frame.h>
}

#include <memory>

namespace gaia::engine {

struct Frame {
    AVFrame *raw;
    
    Frame() {
        this->raw = av_frame_alloc();
    }
    
    ~Frame() {
        av_frame_free(&raw);
    }
};

using FramePtr = std::shared_ptr<Frame>;


static FramePtr makeFrame() {
    return std::make_shared<Frame>();
}

}

#endif /* frame_hpp */

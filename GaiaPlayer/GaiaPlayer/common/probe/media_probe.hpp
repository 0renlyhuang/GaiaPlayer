//
//  media_probe.hpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/8/14
//  
//
    

#ifndef media_probe_hpp
#define media_probe_hpp

#include "engine/engine_env.hpp"
#include "common/frame.hpp"

namespace gaia::engine {

class MediaProbe {
public:
    MediaProbe(std::shared_ptr<EngineEnv> env);
    void printMediaInfo(AVFormatContext *ctx, AVCodecContext *v_codec_ctx, AVCodecContext *a_codec_ctx);
    
    void onVFrame(FramePtr frame);
    void onAFrame(FramePtr frame);
    
    
private:
    std::shared_ptr<EngineEnv> env_;
    
    std::once_flag never_recv_v_frame_;
    std::once_flag never_recv_a_frame_;
    bool is_ever_recv_v_frame_ = false;
    bool is_ever_recv_a_frame_ = false;
};

}

#endif /* media_probe_hpp */

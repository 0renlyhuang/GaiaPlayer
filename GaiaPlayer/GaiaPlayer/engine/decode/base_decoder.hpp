//
//  base_decoder.hpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/7/26
//  
//
    

#ifndef base_decoder_hpp
#define base_decoder_hpp


#include "engine/engine_env.hpp"
#include "engine/decorate/audio_unifier.hpp"
#include "engine/destination/sdl_dst.hpp"
#include "base/error/error_str.hpp"
#include "common/packet.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/frame.h>
#include <libavutil/time.h>
#include <libavutil/imgutils.h>
}

#include <folly/experimental/coro/Task.h>

#include <memory>
#include <optional>
#include <string>

namespace gaia::engine {

class BaseDecoder {
public:
    BaseDecoder();
    virtual ~BaseDecoder() = default;
    
    base::ErrorMsgOpt openStream(AVFormatContext *fmt_ctx, AVStream *stream);
    
    virtual base::ErrorMsgOpt openConcreteStream(AVCodecContext *avctx, AVFormatContext *fmt_ctx, AVStream *stream) = 0;
    
    virtual bool isCacheEnough() = 0;
    virtual bool isCacheInNeed() = 0;
    
    virtual base::ErrorMsgOpt decode(PacketPtr pkt) = 0;
    
    
    
protected:
    AVCodecContext *avctx_;
    int64_t start_pts_ = AV_NOPTS_VALUE;
    AVRational start_pts_tb_ = {};
};

}

#endif /* base_decoder_hpp */

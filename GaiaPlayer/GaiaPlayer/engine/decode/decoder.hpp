//
//  decoder.hpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/7/9
//  
//
    

#ifndef decoder_hpp
#define decoder_hpp

#include "engine/engine_env.hpp"
#include "engine/decorate/audio_unifier.hpp"
#include "engine/destination/sdl_dst.hpp"
#include "engine/decode/audio/audio_decoder.hpp"
#include "engine/decode/video/video_decoder.hpp"
#include "base/error/error_str.hpp"


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

class Decoder {
public:
    Decoder(std::shared_ptr<EngineEnv> env, std::shared_ptr<AudioDecoder> audio_decoder, std::shared_ptr<VideoDecoder> video_decoder);
    
    base::ErrorMsgOpt openStreams(AVFormatContext *p_fmt_ctx, std::shared_ptr<Streams> streams);
    
    bool isCacheEnough();
    bool isCacheInNeed();
    std::string getCacheTrackInfo();
    
    base::ErrorMsgOpt decode(PacketPtr pkt);
    
    
    void handlePktEnd();
    
private:
    std::shared_ptr<EngineEnv> env_;
    std::shared_ptr<AudioDecoder> audio_decoder_;
    std::shared_ptr<VideoDecoder> video_decoder_;
};

}

#endif /* decoder_hpp */

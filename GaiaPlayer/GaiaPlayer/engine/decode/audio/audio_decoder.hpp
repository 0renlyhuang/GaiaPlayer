//
//  audio_decoder.hpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/7/26
//  
//
    

#ifndef audio_decoder_hpp
#define audio_decoder_hpp

#include "engine/engine_env.hpp"
#include "engine/decorate/audio_unifier.hpp"
#include "engine/destination/sdl_dst.hpp"
#include "engine/decode/base_decoder.hpp"
#include "engine/decode/decoded_content.hpp"
#include "base/buf/buf.hpp"
#include "base/async/executors.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/frame.h>
#include <libavutil/time.h>
#include <libavutil/imgutils.h>
}

#include <queue>
#include <folly/concurrency/UnboundedQueue.h>

namespace gaia::engine {

class AudioDecoder: public BaseDecoder {
    friend class ConsumeDataSource;
public:
    AudioDecoder(std::shared_ptr<EngineEnv> env, std::shared_ptr<AudioUnifier> audio_unifier, std::shared_ptr<SDLDst> sdl_dst);
    ~AudioDecoder() override = default;
    
    
    
    std::optional<base::ErrorMsg> openConcreteStream(AVCodecContext *avctx, AVFormatContext *fmt_ctx, AVStream *stream) override;
    
    bool isCacheEnough() override;
    bool isCacheInNeed() override;
    
    base::ErrorMsgOpt decode(PacketPtr pkt) override;
    
    
private:
    base::ErrorMsgOpt filterFrame(FramePtr frame, PacketPtr pkt);
    
    std::shared_ptr<EngineEnv> env_;
    std::shared_ptr<AudioUnifier> audio_unifier_;
    std::shared_ptr<SDLDst> sdl_dst_;
    
//    std::queue<DecodedFrame> queue_;  // USPSCQueue
    folly::USPSCQueue<DecodedFrame, false> queue_;
    
    int64_t next_pts_ = AV_NOPTS_VALUE;
    AVRational tb_of_next_pts_;
};

}

#endif /* audio_decoder_hpp */

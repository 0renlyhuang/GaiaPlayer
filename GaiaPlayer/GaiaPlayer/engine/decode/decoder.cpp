//
//  decoder.cpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/7/9
//  
//
    

#include "decoder.hpp"

#include "common/packet.hpp"

extern "C" {
#include <libavutil/dict.h>
#include <libavutil/opt.h>
#include <libavfilter/avfilter.h>
}


#include <folly/Format.h>
#include <folly/experimental/coro/Task.h>
#include <folly/ScopeGuard.h>
#include <folly/experimental/coro/Sleep.h>

#include <chrono>

namespace gaia::engine {


Decoder::Decoder(std::shared_ptr<EngineEnv> env, std::shared_ptr<AudioDecoder> audio_decoder, std::shared_ptr<VideoDecoder> video_decoder): env_(env),  audio_decoder_(audio_decoder), video_decoder_(video_decoder) {}


base::ErrorMsgOpt Decoder::openStreams(AVFormatContext *p_fmt_ctx, std::shared_ptr<Streams> streams) {
    if (streams->a_stream) {
        const auto error = this->audio_decoder_->openStream(p_fmt_ctx, streams->a_stream);
        if (error.has_value()) {
            return error;
        }
    }
    
    if (streams->v_stream) {
        const auto error = this->video_decoder_->openStream(p_fmt_ctx, streams->v_stream);
        if (error.has_value()) {
            return error;
        }
    }
    
    return base::noError;
}

bool Decoder::isCacheEnough() {
    return false;  // TODO
}

base::ErrorMsgOpt Decoder::decode(PacketPtr pkt) {
    if (pkt->raw->stream_index == this->env_->streams->a_idx) {
        return this->audio_decoder_->decode(pkt);
    }
    else if (pkt->raw->stream_index == this->env_->streams->v_idx) {
        return this->video_decoder_->decode(pkt);
    }
    
    return base::noError;
}

}

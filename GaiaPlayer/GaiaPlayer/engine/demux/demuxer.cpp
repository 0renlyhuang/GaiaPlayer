//
//  demuxer.cpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/7/6
//  
//
    

#include "demuxer.hpp"

#include <folly/Format.h>
#include <folly/Expected.h>
#include <folly/experimental/coro/Sleep.h>
#include <folly/io/async/EventBaseManager.h>

#include "base/log/log.hpp"
#include <folly/logging/xlog.h>

#include <chrono>

namespace gaia::engine {

folly::Expected<Streams, base::ErrorMsg> Demuxer::splitStreams(AVFormatContext *p_fmt_ctx) {
    int a_idx = -1;
    int v_idx = -1;
    AVStream *p_audio_stream = nullptr;
    AVStream *p_video_stream = nullptr;
    
    for (int i=0; i < p_fmt_ctx->nb_streams; ++i)
    {
        if ((p_fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) &&
            (a_idx == -1))
        {
            a_idx = i;
            p_audio_stream = p_fmt_ctx->streams[i];
            XLOG(INFO) << folly::sformat("Find a audio stream, index {}", a_idx);
        }
        if ((p_fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) &&
            (v_idx == -1))
        {
            v_idx = i;
            p_video_stream = p_fmt_ctx->streams[i];
            XLOG(INFO) << folly::sformat("Find a video stream, index {}", v_idx);
        }
        if (a_idx != -1 && v_idx != -1)
        {
            break;
        }
    }
    
    if (a_idx == -1 && v_idx == -1) {
        return folly::makeUnexpected("Cann't find any audio/video stream");
    }
    
    
    return Streams{.a_stream = p_audio_stream, .v_stream = p_video_stream, a_idx = a_idx, v_idx = v_idx };
}


Demuxer::Demuxer(std::shared_ptr<EngineEnv> env, std::shared_ptr<gaia::base::Executors> executor, std::shared_ptr<Decoder> decoder)
    : env_(env), executor_(executor), decoder_(decoder), demux_cancel_source_(folly::CancellationSource()), is_eof_(false) {
    
}

base::ErrorMsgOpt Demuxer::openStreams(AVFormatContext *p_fmt_ctx, std::shared_ptr<Streams> streams) {
    return this->decoder_->openStreams(p_fmt_ctx, streams);
}


void Demuxer::startDemux(int32_t serial) {
    XLOG(DBG) << "Demuxer::startDemux";

    const auto when_finished = folly::coro::co_withCancellation(this->demux_cancel_source_.getToken(), [this, serial]{
        return this->demuxLoop(serial);
    })()
        .scheduleOn(this->executor_->Logic().get())
        .start();
    
}

folly::coro::Task<> Demuxer::demuxLoop(int32_t serial) {
    XLOG(DBG) << "Demuxer::demuxLoop";
    const auto isCacheEnough = this->decoder_->isCacheEnough();
    if (isCacheEnough) {
        XLOG(INFO) << "cache enough, stop demux";
        this->is_demux_looping_ = false;
        co_return;
    }
    
    if (serial != this->env_->serial) {
        co_return;
    }
    
    if (this->is_eof_) {
        co_return;
    }
    

    this->is_demux_looping_ = true;
    const auto produce_result = this->produceOnce();
    if (produce_result.hasError()) {
        this->is_demux_looping_ = false;
        co_return;
    }
    
    if (produce_result.value().is_eof) {
        this->is_eof_ = true;
        this->is_demux_looping_ = false;
        co_return;
    }
    
    if (produce_result.value().need_delay_retry) {
        co_await folly::coro::sleep(std::chrono::milliseconds(10));
    }
    
    const auto when_finished = folly::coro::co_withCancellation(this->demux_cancel_source_.getToken(), [this, serial]{
        return this->demuxLoop(serial);
    })()
        .scheduleOn(this->executor_->Logic().get())
        .start();
}

void Demuxer::pauseDemux() {
    if (!this->is_demux_looping_) {
        return;
    }
    
    this->demux_cancel_source_.requestCancellation();
    this->demux_cancel_source_ = {};
    this->is_demux_looping_ = false;
}


folly::Expected<Demuxer::ProduceResult, base::ErrorMsg> Demuxer::produceOnce() {
    constexpr auto needDelayRetry = true;
    constexpr auto noNeedToDelay = false;
    
    PacketPtr pkt = makePacket();
    if (!pkt->raw) {
        return base::unexpected("Could not allocate packet.");
    }
    
    
    auto *ctx = this->env_->ctx;
    int ret = av_read_frame(ctx, pkt->raw);
    
    if (ret < 0) {
        if ((ret == AVERROR_EOF || avio_feof(ctx->pb))) {
            XLOG(INFO, "av_read_frame is_eof");
            return ProduceResult{ .is_eof=true, .need_delay_retry=false };
        }
        
        if (ctx->pb && ctx->pb->error) {
            return base::unexpected("av_read_frame error:{}.", ctx->pb->error);
        }
        
        // when error happends, delay 10ms to produce
        return ProduceResult{ .is_eof=false, .need_delay_retry=true };
    }
    
    [[maybe_unused]] const auto decode_error = this->decoder_->decode(pkt);
    
    return ProduceResult{ .is_eof=false, .need_delay_retry=false };
}

}

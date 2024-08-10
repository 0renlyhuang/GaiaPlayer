//
//  audio_decoder.cpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/7/26
//  
//
    

#include "audio_decoder.hpp"

#include "base/error/error_str.hpp"
#include "common/frame.hpp"

#include <folly/Format.h>
#include <folly/ScopeGuard.h>

namespace gaia::engine {


AudioDecoder::AudioDecoder(std::shared_ptr<EngineEnv> env, std::shared_ptr<AudioUnifier> audio_unifier, std::shared_ptr<SDLDst> sdl_dst)
    : BaseDecoder(), env_(env), audio_unifier_(audio_unifier), sdl_dst_(sdl_dst) {
}
    
std::optional<base::ErrorMsg> AudioDecoder::openConcreteStream(AVCodecContext *avctx, AVFormatContext *fmt_ctx, AVStream *stream) {
    auto filter_open_rlt = this->audio_unifier_->openFilter(avctx);
    if (filter_open_rlt.hasError()) {
        return filter_open_rlt.error();
    }
    
    auto sink = this->audio_unifier_->filter_sink_;
    const auto sink_sample_rate = av_buffersink_get_sample_rate(sink);
      
    AVChannelLayout sink_ch_layout = { };
    int ret = av_buffersink_get_ch_layout(sink, &sink_ch_layout);
    if (ret < 0) {
        return base::error("av_buffersink_get_ch_layout faild, ret:{}", ret);
    }
    auto sink_ch_layout_guard = folly::makeGuard([&]{ av_channel_layout_uninit(&sink_ch_layout); });
    
    const auto device_rlt = this->sdl_dst_->openAudioDevice(&sink_ch_layout, sink_sample_rate);
    if (device_rlt.hasError()) {
        return device_rlt.error();
    }
        
        


//        is->audio_hw_buf_size = ret;
//        is->audio_src = is->audio_tgt;
//        is->audio_buf_size  = 0;
//        is->audio_buf_index = 0;
//
//        /* init averaging filter */
//        is->audio_diff_avg_coef  = exp(log(0.01) / AUDIO_DIFF_AVG_NB);
//        is->audio_diff_avg_count = 0;
//        /* since we do not have a precise anough audio FIFO fullness,
//           we correct audio sync only if larger than this threshold */
//        is->audio_diff_threshold = (double)(is->audio_hw_buf_size) / is->audio_tgt.bytes_per_sec;

//        is->audio_stream = stream_index;
//        is->audio_st = ic->streams[stream_index];
    

//    if ((ret = decoder_init(&is->auddec, avctx, &is->audioq, is->continue_read_thread)) < 0)
//        goto fail;
    if (fmt_ctx->iformat->flags & AVFMT_NOTIMESTAMPS) {
        this->start_pts_ = stream->start_time;
        this->start_pts_tb_ = stream->time_base;
    }
//    if ((ret = decoder_start(&is->auddec, audio_thread, "audio_decoder", is)) < 0)
//        goto out;
    
    return std::nullopt;
}


bool AudioDecoder::isCacheEnough() {
    return false;
}

bool AudioDecoder::isCacheInNeed() {
    return true;
}

base::ErrorMsgOpt AudioDecoder::filterFrame(FramePtr frame, PacketPtr pkt) {
    // TODO: reconfigure filter when auido frame layout changed
    
    // push frame to filter
    int ret = av_buffersrc_add_frame(this->audio_unifier_->filter_src_, frame->raw);
    if (ret < 0) {
        return base::error("av_buffersrc_add_frame faild. {}", av_err2str(ret));
    }
    
    // get new frame from filter
    while (true) {
//            if (is->audioq.serial != is->auddec.pkt_serial)
//                break;
            
        FramePtr frame_unified = makeFrame();
        ret = av_buffersink_get_frame_flags(this->audio_unifier_->filter_sink_, frame_unified->raw, 0);
        
        if (ret == AVERROR(AVERROR_EOF)) {
            break;
        }
        else if (ret == AVERROR(EAGAIN)) {
            break;
        }
        else if (ret < 0) {
            return base::error("av_buffersink_get_frame_flags faild. {}", av_err2str(ret));
        }
        
        
        const AVRational tb = av_buffersink_get_time_base(this->audio_unifier_->filter_sink_);
        auto pts = std::optional<base::TimeUnit>();
        if (frame_unified->raw->pts != AV_NOPTS_VALUE) {
            pts = base::toTimeUnit(frame_unified->raw->pts, tb);
        }
        
        const int64_t pos = pkt->raw->pos;
        const int serial = this->env_->serial;
        const auto duration = base::toTimeUnit(1, (AVRational){frame_unified->raw->nb_samples, frame_unified->raw->sample_rate});
        
        
        this->queue_.enqueue(DecodedFrame{ .frame=frame_unified, .source_pkt=pkt, .pts=pts, .pos=pos, .serial=serial, .duration=duration });
    }
    
    return base::noError;
}

base::ErrorMsgOpt AudioDecoder::decode(PacketPtr pkt) {
    const auto error = avcodec_send_packet(this->avctx_, pkt->raw);
    if (error) {
        return base::error("avcodec_send_packet faild. {}", av_err2str(error));
    }

    while (true) {
        FramePtr frame = makeFrame();
        int ret = avcodec_receive_frame(this->avctx_, frame->raw);

        if (ret == AVERROR_EOF) {
//                d->finished = d->pkt_serial;
            avcodec_flush_buffers(this->avctx_);
            break;
        }
        else if (ret == AVERROR(EAGAIN)) {
            break;
        }
        if (ret < 0) {
            return base::error("audio avcodec_receive_frame faild. {}", av_err2str(ret));
        }
        
        if (ret >= 0) {
            AVRational tb = (AVRational){1, frame->raw->sample_rate};
            if (frame->raw->pts != AV_NOPTS_VALUE)
                frame->raw->pts = av_rescale_q(frame->raw->pts, this->avctx_->pkt_timebase, tb);
            else if (this->next_pts_ != AV_NOPTS_VALUE)
                frame->raw->pts = av_rescale_q(this->next_pts_, this->tb_of_next_pts_, tb);
            
            if (frame->raw->pts != AV_NOPTS_VALUE) {
                this->next_pts_ = frame->raw->pts + frame->raw->nb_samples;
                this->tb_of_next_pts_ = tb;
            }
        }
        
        const auto error = this->filterFrame(frame, pkt);
        if (error.has_value()) {
            continue;
        }
        
    }
    
    return base::noError;
}
    





}

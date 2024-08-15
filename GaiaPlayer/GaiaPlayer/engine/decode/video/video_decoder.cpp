//
//  video_decoder.cpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/7/27
//  
//
    

#include "video_decoder.hpp"

#include "base/error/error_str.hpp"
#include "base/log/log.hpp"
#include <format>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/frame.h>
#include <libavutil/time.h>
#include <libavutil/imgutils.h>
}

namespace gaia::engine {

VideoDecoder::VideoDecoder(std::shared_ptr<EngineEnv> env, std::shared_ptr<VideoUnifier> video_unifier, std::shared_ptr<EventCenter> event_center, std::shared_ptr<MediaProbe> media_probe): BaseDecoder(), env_(env), video_unifier_(video_unifier), event_center_(event_center), media_probe_(media_probe) {
    
}


std::optional<std::string> VideoDecoder::openConcreteStream(AVCodecContext *avctx, AVFormatContext *fmt_ctx, AVStream *stream) {
    
//    is->video_stream = stream_index;
//    is->video_st = ic->streams[stream_index];
//
//    if ((ret = decoder_init(&is->viddec, avctx, &is->videoq, is->continue_read_thread)) < 0)
//        goto fail;
//    if ((ret = decoder_start(&is->viddec, video_thread, "video_decoder", is)) < 0)
//        goto out;
//    is->queue_attachments_req = 1;
    
    return base::noError;
}

base::ErrorMsgOpt VideoDecoder::decode(PacketPtr pkt) {
    const auto error = avcodec_send_packet(this->avctx_, pkt->raw);
    if (error) {
        return base::error("video avcodec_send_packet faild. {}", error);
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
            XLOG(DBG) << "decode EAGAIN";
            break;
        }
        if (ret < 0) {
            return base::error("video avcodec_receive_frame faild. {}", av_err2str(ret));
        }
        
        constexpr auto decoder_reorder_pts = -1;
        if (decoder_reorder_pts == -1) {
            frame->raw->pts = frame->raw->best_effort_timestamp;
        } else if (!decoder_reorder_pts) {
            frame->raw->pts = frame->raw->pkt_dts;
        }

        frame->raw->sample_aspect_ratio = av_guess_sample_aspect_ratio(this->env_->ctx, this->env_->streams->v_stream, frame->raw);
        this->media_probe_->onVFrame(frame);
        
        const auto error = this->filterFrame(frame, pkt);
        if (error.has_value()) {
            continue;
        }
    }
    
    return base::noError;
}

base::ErrorMsgOpt VideoDecoder::filterFrame(FramePtr frame, PacketPtr pkt) {
    // TODO: reconfigure filter when auido frame layout changed
    const auto error = this->video_unifier_->recofigIfNeeded(frame);
    if (error.has_value()) {
        return error;
    }
    
    // push frame to filter
    int ret = av_buffersrc_add_frame(this->video_unifier_->filter_src_, frame->raw);
    if (ret < 0) {
        return base::error("av_buffersrc_add_frame faild. {}", av_err2str(ret));
    }
    
    
    AVRational frame_rate = av_buffersink_get_frame_rate(this->video_unifier_->filter_sink_);
    AVRational tb = av_buffersink_get_time_base(this->video_unifier_->filter_sink_);
    
    
    // get new frame from filter
    while (true) {
        FramePtr frame_unified = makeFrame();
        ret = av_buffersink_get_frame_flags(this->video_unifier_->filter_sink_, frame_unified->raw, 0);
        if (ret == AVERROR(AVERROR_EOF)) {
            break;
        }
        else if (ret == AVERROR(EAGAIN)) {
            XLOG(DBG) << "filterFrame EAGAIN";
            break;
        }
        else if (ret < 0) {
            return base::error("av_buffersink_get_frame_flags faild. {}", av_err2str(ret));
        }
        
        
//            FrameData *fd;
//
//             is->frame_last_returned_time = av_gettime_relative() / 1000000.0;
//
//             fd = frame->opaque_ref ? (FrameData*)frame->opaque_ref->data : NULL;
//
//             is->frame_last_filter_delay = av_gettime_relative() / 1000000.0 - is->frame_last_returned_time;
//             if (fabs(is->frame_last_filter_delay) > AV_NOSYNC_THRESHOLD / 10.0)
//                 is->frame_last_filter_delay = 0;
//             tb = av_buffersink_get_time_base(filt_out);
        
        auto duration = base::TimeUnit(0);
        if (frame_rate.num && frame_rate.den) {
            duration = base::toTimeUnit(1, (AVRational){frame_rate.den, frame_rate.num});
        }
        auto pts = std::optional<base::TimeUnit>();
        if (frame_unified->raw->pts != AV_NOPTS_VALUE) {
            pts = base::toTimeUnit(frame_unified->raw->pts, tb);
        }
        const int64_t pos = pkt->raw->pos;
        const int serial = this->env_->serial;
        
        
        const auto decode_frame = std::make_shared<DecodedFrame>(DecodedFrame{ .frame=frame_unified, .source_pkt=pkt, .pts=pts, .pos=pos, .serial=serial, .duration=duration });
        this->queue_.push(decode_frame);
        XLOG(DBG, std::format("push video frame, pts:{}, duration:{}", base::toSecStr(pts.value()), base::toSecStr(duration)));
        
        if (last_decoded_frame_.has_value()) {
            this->updateLastFrameDuration(last_decoded_frame_.value(), decode_frame);
        }
        last_decoded_frame_ = decode_frame;
        
        if (this->queue_.size() == 1) {
            this->event_center_->video_frame_produce_notify.setValue(folly::Unit());
        }
        
    }
    
    return base::noError;
}

void VideoDecoder::updateLastFrameDuration(DecodedFramePtr last_frame, DecodedFramePtr curr_frame) {
    if (!last_frame->pts.has_value() || !curr_frame->pts.has_value()) {
        return;
    }
    
    const auto last_pts = last_frame->pts.value();
    const auto curr_pts = curr_frame->pts.value();
    if (last_pts > curr_pts) {
        return;
    }
    
    if (last_pts + this->env_->max_frame_duration < curr_pts) {
        return;
    }
    
    const auto duration = curr_pts - last_pts;
    XLOG(DBG, std::format("update last video frame duration, old:{}, new duration:{}", base::toSecStr(last_frame->duration), base::toSecStr(duration)));
    last_frame->duration = duration;
}

std::optional<base::TimeUnit> VideoDecoder::getDurationInQueue() {
    if (this->queue_.empty()) {
        return std::nullopt;
    }
    
    const auto first_pts_opt = this->queue_.front()->pts;
    const auto last_pts_opt =  this->queue_.back()->pts;
    
    if (first_pts_opt.has_value() && last_pts_opt.has_value()) {
        const auto first_pts = first_pts_opt.value();
        const auto last_pts = last_pts_opt.value();
        if (last_pts > first_pts) {
            const auto duration = last_pts -  first_pts;
            return duration;
        }
    }
    
    return std::nullopt;
}

bool VideoDecoder::isCacheEnough() {
    const auto duration_in_queue = this->getDurationInQueue();
    if (!duration_in_queue.has_value()) {
        return false;
    }
    
    using namespace std::chrono_literals;
    return duration_in_queue.value() >= 10s;
}

bool VideoDecoder::isCacheInNeed() {
    const auto duration_in_queue = this->getDurationInQueue();
    if (!duration_in_queue.has_value()) {
        return true;
    }
    
    using namespace std::chrono_literals;
    return duration_in_queue.value() <= 5s;
}

}


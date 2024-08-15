//
//  media_probe.cpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/8/14
//  
//
    

#include "media_probe.hpp"

#include "base/log/log.hpp"
#include "base/time/time_unit.hpp"

namespace gaia::engine {


MediaProbe::MediaProbe(std::shared_ptr<EngineEnv> env): env_(env) {}

void MediaProbe::printMediaInfo(AVFormatContext *ctx, AVCodecContext *v_codec_ctx, AVCodecContext *a_codec_ctx) {
    auto v_info = std::string();
    auto a_info = std::string();
    
    auto v_stream_idx = 0;
    auto a_stream_idx = 0;
    
    for (unsigned int i = 0; i < ctx->nb_streams; i++) {
        AVStream *stream = ctx->streams[i];
        AVCodecParameters *codecpar = stream->codecpar;
        
        auto duration_str = std::string("unknwon");
        if (stream->duration != AV_NOPTS_VALUE) {
            const auto duration = base::toTimeUnit(stream->duration, stream->time_base);
            duration_str = std::format("{:%T}", duration);
        }
        
        
        if (codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            AVRational frame_rate = stream->avg_frame_rate;
            int64_t bit_rate = codecpar->bit_rate;
            if (bit_rate == 0) {
                bit_rate = ctx->bit_rate;
            }
            
            auto pix_fmt = std::string("unknown");
            if (v_codec_ctx->codec_id == codecpar->codec_id && v_codec_ctx->pix_fmt != AV_PIX_FMT_NONE) {
                pix_fmt = av_get_pix_fmt_name(v_codec_ctx->pix_fmt);
            }
            else if ((AVPixelFormat)codecpar->format != AV_PIX_FMT_NONE) {
                pix_fmt = av_get_pix_fmt_name((AVPixelFormat)codecpar->format);
            }
            
            auto a_v_info = std::format("v, idx:{}, codec:{}, pix_fmt:{}, resolution:{}x{} frame_rate:{:.2f}fps, bitrate:{}kbps, duration:{}, has_b_frame:{}\n",
                                        v_stream_idx,
                                        std::string(avcodec_get_name(codecpar->codec_id)),
                                        pix_fmt,
                                        codecpar->width, codecpar->height,
                                        av_q2d(frame_rate),
                                        bit_rate / 1000,
                                        duration_str,
                                        v_codec_ctx->has_b_frames
                                        );
            v_info += a_v_info;
            ++v_stream_idx;
        }
        else if (codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            char layout_buf[256] = {0};
            av_channel_layout_describe(&codecpar->ch_layout, layout_buf, sizeof(layout_buf));
            
            auto sample_fmt = std::string("unknown");
            if (a_codec_ctx->codec_id == codecpar->codec_id && a_codec_ctx->sample_fmt != AV_SAMPLE_FMT_NONE) {
                sample_fmt = av_get_sample_fmt_name(a_codec_ctx->sample_fmt);
            }
            else if ((AVSampleFormat)codecpar->format != AV_SAMPLE_FMT_NONE) {
                sample_fmt = av_get_sample_fmt_name((AVSampleFormat)codecpar->format);
            }
            
            
            auto a_a_info = std::format("a, idx:{}: codec:{}, smpl_fmt:{}, layout:{}, channels:{}, smpl_rate:{}Hz, bitrate:{}kbps, duration:{}\n",
                                        a_stream_idx,
                                        avcodec_get_name(codecpar->codec_id),
                                        sample_fmt,
                                        std::string(layout_buf),
                                        codecpar->ch_layout.nb_channels,
                                        codecpar->sample_rate,
                                        codecpar->bit_rate / 1000,
                                        duration_str
                                        );
            a_info += a_a_info;
            ++a_stream_idx;
        }
    }
    
    XLOG(INFO) << std::format("[Media] file:{}, format_name:{}, format_long_name:{}, streams:{}, bit_rate:{}",
                              env_->file, ctx->iformat->name, ctx->iformat->long_name, ctx->nb_streams, ctx->bit_rate);
    XLOGF(INFO, "[Media] {}", v_info);
    XLOGF(INFO, "[Media] {}", a_info);
    
    
}

void MediaProbe::onVFrame(FramePtr frame) {
    std::call_once(this->never_recv_v_frame_, [=]{
        auto pix_fmt = std::string("unknown");
        if (AVPixelFormat(frame->raw->format) != AV_PIX_FMT_NONE) {
            pix_fmt = av_get_pix_fmt_name(AVPixelFormat(frame->raw->format));
        }
        
        auto color_space = std::string("unknown");
        if (frame->raw->colorspace != AVCOL_SPC_UNSPECIFIED) {
            color_space = av_color_space_name(frame->raw->colorspace );
        }
        
        auto color_primaries = std::string("unknown");
        if (frame->raw->color_primaries != AVCOL_PRI_UNSPECIFIED) {
            color_primaries = av_color_primaries_name(frame->raw->color_primaries);
        }
        
        auto color_transfer = std::string("unknown");
        if (frame->raw->color_trc != AVCOL_TRC_UNSPECIFIED) {
            color_primaries = av_color_transfer_name(frame->raw->color_trc);
        }
        
        XLOGF(INFO, "[Media] v, pix_fmt:{}, color_space:{}, color_primaries:{}, color_trc:{}", pix_fmt, color_space, color_primaries, color_transfer);
    });
}

void MediaProbe::onAFrame(FramePtr frame) {
    
}


}

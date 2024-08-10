//
//  video_unifier.cpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/8/1
//  
//
    

#include "video_unifier.hpp"

#include "engine/decorate/graph_helper.hpp"

#include <SDL2/SDL_pixels.h>

namespace gaia::engine {




VideoUnifier::VideoUnifier(std::shared_ptr<EngineEnv> env, std::shared_ptr<SDLDst> sdl_dst): env_(env), sdl_dst_(sdl_dst) {
    
}


std::optional<base::ErrorMsg> VideoUnifier::recofigIfNeeded(FramePtr frame) {
    if (   this->last_w_ == frame->raw->width
        && this->last_h_ == frame->raw->height
        && this->last_format_ == frame->raw->format)
//        && this->last_serial_ == is->viddec.pkt_serial
//        && this->last_vfilter_idx_ == is->vfilter_idx)
    {
        return std::nullopt;
    }
    
    
    
    avfilter_graph_free(&this->graph_);
    this->graph_ = avfilter_graph_alloc();
    if (!this->graph_) {
        return base::error("avfilter_graph_alloc faild");
    }
    
    auto graph_guard = folly::makeGuard([this]{ avfilter_graph_free(&this->graph_); });
    
//        this->graph_->nb_threads = filter_nbthreads;
    
    int ret = configureVideoFiltersGraph(this->graph_, nullptr/*vfilters_list ? vfilters_list[is->vfilter_idx] : NULL*/, frame->raw);
    if (ret < 0) {
        return base::error("configureVideoFiltersGraph faild. should quit.");
    }
    
    
    this->last_w_ = frame->raw->width;
    this->last_h_ = frame->raw->height;
    this->last_format_ = AVPixelFormat(frame->raw->format);
//    this->last_serial_ = is->viddec.pkt_serial;
//    this->last_vfilter_idx_ = is->vfilter_idx;
    
    
    graph_guard.dismiss();
    return base::noError;
}

int VideoUnifier::configureVideoFiltersGraph(AVFilterGraph *graph, const char *vfilters, AVFrame *frame) {
    AVStream *v_stream = this->env_->streams->v_stream;
    
    AVPixelFormat *pix_fmts = this->sdl_dst_->getSupportPixFmts();
    
    char sws_flags_str[512] = "";
    char buffersrc_args[256];
    int ret;
    AVFilterContext *filt_src = NULL, *filt_out = NULL, *last_filter = NULL;
    AVCodecParameters *codecpar = v_stream->codecpar;
    AVRational fr = av_guess_frame_rate(this->env_->ctx, v_stream, NULL);
    const AVDictionaryEntry *e = NULL;
    int nb_pix_fmts = 0;
    int i, j;
    AVBufferSrcParameters *par = av_buffersrc_parameters_alloc();

    if (!par)
        return AVERROR(ENOMEM);

//    while ((e = av_dict_iterate(sws_dict, e))) {
//        if (!strcmp(e->key, "sws_flags")) {
//            av_strlcatf(sws_flags_str, sizeof(sws_flags_str), "%s=%s:", "flags", e->value);
//        } else
//            av_strlcatf(sws_flags_str, sizeof(sws_flags_str), "%s=%s:", e->key, e->value);
//    }
    if (strlen(sws_flags_str))
        sws_flags_str[strlen(sws_flags_str)-1] = '\0';

    graph->scale_sws_opts = av_strdup(sws_flags_str);

    snprintf(buffersrc_args, sizeof(buffersrc_args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d:"
             "colorspace=%d:range=%d",
             frame->width, frame->height, frame->format,
             v_stream->time_base.num, v_stream->time_base.den,
             codecpar->sample_aspect_ratio.num, FFMAX(codecpar->sample_aspect_ratio.den, 1),
             frame->colorspace, frame->color_range);
    if (fr.num && fr.den)
        av_strlcatf(buffersrc_args, sizeof(buffersrc_args), ":frame_rate=%d/%d", fr.num, fr.den);

    if ((ret = avfilter_graph_create_filter(&filt_src,
                                            avfilter_get_by_name("buffer"),
                                            "ffplay_buffer", buffersrc_args, NULL,
                                            graph)) < 0)
        goto fail;
    par->hw_frames_ctx = frame->hw_frames_ctx;
    ret = av_buffersrc_parameters_set(filt_src, par);
    if (ret < 0)
        goto fail;

    ret = avfilter_graph_create_filter(&filt_out,
                                       avfilter_get_by_name("buffersink"),
                                       "ffplay_buffersink", NULL, NULL, graph);
    if (ret < 0)
        goto fail;

    if ((ret = av_opt_set_int_list(filt_out, "pix_fmts", pix_fmts,  AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN)) < 0)
        goto fail;
    if (/*!vk_renderer &&*/
        (ret = av_opt_set_int_list(filt_out, "color_spaces", this->sdl_dst_->getSupportColorSpace(),  AVCOL_SPC_UNSPECIFIED, AV_OPT_SEARCH_CHILDREN)) < 0)
        goto fail;

    last_filter = filt_out;

/* Note: this macro adds a filter before the lastly added filter, so the
 * processing order of the filters is in reverse */
#define INSERT_FILT(name, arg) do {                                          \
    AVFilterContext *filt_ctx;                                               \
                                                                             \
    ret = avfilter_graph_create_filter(&filt_ctx,                            \
                                       avfilter_get_by_name(name),           \
                                       "ffplay_" name, arg, NULL, graph);    \
    if (ret < 0)                                                             \
        goto fail;                                                           \
                                                                             \
    ret = avfilter_link(filt_ctx, 0, last_filter, 0);                        \
    if (ret < 0)                                                             \
        goto fail;                                                           \
                                                                             \
    last_filter = filt_ctx;                                                  \
} while (0)

//    if (autorotate) {
//        double theta = 0.0;
//        int32_t *displaymatrix = NULL;
//        AVFrameSideData *sd = av_frame_get_side_data(frame, AV_FRAME_DATA_DISPLAYMATRIX);
//        if (sd)
//            displaymatrix = (int32_t *)sd->data;
//        if (!displaymatrix) {
//            const AVPacketSideData *psd = av_packet_side_data_get(v_stream->codecpar->coded_side_data,
//                                                                  v_stream->codecpar->nb_coded_side_data,
//                                                                  AV_PKT_DATA_DISPLAYMATRIX);
//            if (psd)
//                displaymatrix = (int32_t *)psd->data;
//        }
//        theta = get_rotation(displaymatrix);
//
//        if (fabs(theta - 90) < 1.0) {
//            INSERT_FILT("transpose", "clock");
//        } else if (fabs(theta - 180) < 1.0) {
//            INSERT_FILT("hflip", NULL);
//            INSERT_FILT("vflip", NULL);
//        } else if (fabs(theta - 270) < 1.0) {
//            INSERT_FILT("transpose", "cclock");
//        } else if (fabs(theta) > 1.0) {
//            char rotate_buf[64];
//            snprintf(rotate_buf, sizeof(rotate_buf), "%f*PI/180", theta);
//            INSERT_FILT("rotate", rotate_buf);
//        }
//    }

    
    ret = GraphHelper::ConfigureFiltergraph(this->graph_, nullptr/*afilters*/, filt_src, filt_out);
    if (ret < 0) {
        return ret;
//        return base::unexpected("video ConfigureFiltergraph faild. {}", ret);
    }

    
    this->filter_src_ = filt_src;
    this->filter_sink_ = filt_out;

fail:
    av_freep(&par);
    return ret;
}


}

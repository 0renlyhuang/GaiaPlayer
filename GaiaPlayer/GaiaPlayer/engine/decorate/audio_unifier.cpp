//
//  audio_unifier.cpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/7/23
//  
//
    

#include "audio_unifier.hpp"

#include "engine/decorate/graph_helper.hpp"

extern "C" {
#include <libavutil/dict.h>
#include <libavutil/opt.h>
#include <libavfilter/avfilter.h>
#include <libavutil/bprint.h>
#include <libavutil/channel_layout.h>

#include "libavutil/avstring.h"
#include "libavutil/channel_layout.h"
#include "libavutil/eval.h"
#include "libavutil/mathematics.h"
#include "libavutil/pixdesc.h"
#include "libavutil/imgutils.h"
#include "libavutil/dict.h"
#include "libavutil/fifo.h"
#include "libavutil/parseutils.h"
#include "libavutil/samplefmt.h"
#include "libavutil/time.h"
#include "libavutil/bprint.h"
#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#include "libavutil/opt.h"
#include "libavutil/tx.h"
#include "libswresample/swresample.h"

#include "libavfilter/avfilter.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
}

#include <folly/Format.h>
#include <folly/ScopeGuard.h>

#include "base/error/error_str.hpp"




namespace gaia::engine {
    
folly::Expected<bool, base::ErrorMsg> AudioUnifier::openFilter(AVCodecContext *avctx) {
    
    this->filter_src_config_.freq           = avctx->sample_rate;
        int ret = av_channel_layout_copy(&this->filter_src_config_.ch_layout, &avctx->ch_layout);
        if (ret < 0) {
            return folly::makeUnexpected(folly::sformat("av_channel_layout_copy faild, ret:{}", ret));
        }
            
        this->filter_src_config_.fmt            = avctx->sample_fmt;
    auto configResult = this->ConfigureAudioFilters(std::nullopt);
    if (configResult.hasError()) {
        return base::unexpected(configResult.error());
    }
    
    this->filter_src_ = configResult.value().filter_src;
    this->filter_sink_ = configResult.value().filter_sink;
    
    return true;
}



folly::Expected<AudioUnifier::ConfigureAudioFiltersResult, base::ErrorMsg> AudioUnifier::ConfigureAudioFilters(std::optional<AudioParams *> target_to_align) {
    static const enum AVSampleFormat sample_fmts[] = { AV_SAMPLE_FMT_S16, AV_SAMPLE_FMT_NONE };
        int sample_rates[2] = { 0, -1 };
        AVFilterContext *filt_asrc = NULL, *filt_asink = NULL;
        char aresample_swr_opts[512] = "";
        const AVDictionaryEntry *e = NULL;
        AVBPrint bp;
        char asrc_args[256];
        int ret;
        
    if (this->graph_) {
        avfilter_graph_free(&this->graph_);
    }
        
        if (!(this->graph_ = avfilter_graph_alloc()))
            return base::unexpected("avfilter_graph_alloc faild");
    
    auto graph_guard = folly::makeGuard([this]{ avfilter_graph_free(&this->graph_); });
    
//    this->graph_->nb_threads = filter_nbthreads;

        av_bprint_init(&bp, 0, AV_BPRINT_SIZE_AUTOMATIC);
    auto bp_guard = folly::makeGuard([&] { av_bprint_finalize(&bp, NULL); });

//        while ((e = av_dict_iterate(swr_opts, e)))
//            av_strlcatf(aresample_swr_opts, sizeof(aresample_swr_opts), "%s=%s:", e->key, e->value);
//        if (strlen(aresample_swr_opts))
//            aresample_swr_opts[strlen(aresample_swr_opts)-1] = '\0';
        av_opt_set(this->graph_, "aresample_swr_opts", aresample_swr_opts, 0);

        av_channel_layout_describe_bprint(&this->filter_src_config_.ch_layout, &bp);

        ret = snprintf(asrc_args, sizeof(asrc_args),
                       "sample_rate=%d:sample_fmt=%s:time_base=%d/%d:channel_layout=%s",
                       this->filter_src_config_.freq, av_get_sample_fmt_name(this->filter_src_config_.fmt),
                       1, this->filter_src_config_.freq, bp.str);

        ret = avfilter_graph_create_filter(&filt_asrc,
                                           avfilter_get_by_name("abuffer"), "ffplay_abuffer",
                                           asrc_args, NULL, this->graph_);
    if (ret < 0) {
        return base::unexpected("avfilter_graph_create_filter abuffer faild");
    }


        ret = avfilter_graph_create_filter(&filt_asink,
                                           avfilter_get_by_name("abuffersink"), "ffplay_abuffersink",
                                           NULL, NULL, this->graph_);
    if (ret < 0) {
        return base::unexpected("avfilter_graph_create_filter abuffersink faild");
    }

    ret = av_opt_set_int_list(filt_asink, "sample_fmts", sample_fmts,  AV_SAMPLE_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        return base::unexpected("av_opt_set_int_list faild");
    }
    
    ret = av_opt_set_int(filt_asink, "all_channel_counts", 1, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        return base::unexpected("av_opt_set_int faild");
    }
    
    if (target_to_align.has_value()) {
        const auto target = target_to_align.value();
        
            av_bprint_clear(&bp);
            av_channel_layout_describe_bprint(&target->ch_layout, &bp);
            sample_rates   [0] = target->freq;
        
        
        ret = av_opt_set_int(filt_asink, "all_channel_counts", 0, AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            return base::unexpected("av_opt_set_int all_channel_counts faild");
        }
        
        ret = av_opt_set(filt_asink, "ch_layouts", bp.str, AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            return base::unexpected("av_opt_set ch_layouts faild");
        }
        
        ret = av_opt_set_int_list(filt_asink, "sample_rates"   , sample_rates   ,  -1, AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            return base::unexpected("av_opt_set_int_list sample_rates faild");
        }
               
    }

    ret = GraphHelper::ConfigureFiltergraph(this->graph_, nullptr/*afilters*/, filt_asrc, filt_asink);
    if (ret < 0) {
        return base::unexpected("ConfigureFiltergraph faild");
    }
    
    
    bp_guard.dismiss();
    graph_guard.dismiss();
    
    return ConfigureAudioFiltersResult{ .filter_src=filt_asrc, .filter_sink=filt_asink };
}



}

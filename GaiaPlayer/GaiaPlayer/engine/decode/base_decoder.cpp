//
//  base_decoder.cpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/7/26
//  
//
    

#include "base_decoder.hpp"

#include "base/error/error_str.hpp"

extern "C" {
#include <libavutil/dict.h>
#include <libavutil/opt.h>
#include <libavfilter/avfilter.h>
}


#include <folly/Format.h>


namespace gaia::engine {

BaseDecoder::BaseDecoder() {}

std::optional<base::ErrorMsg> BaseDecoder::openStream(AVFormatContext *fmt_ctx, AVStream *stream) {
    this->avctx_ = avcodec_alloc_context3(NULL);
    if (!avctx_)
        return base::error("avcodec_alloc_context3 fail");
    
    auto avctx_guard = folly::makeGuard([&]{ avcodec_free_context(&avctx_); });
    
    int ret = avcodec_parameters_to_context(avctx_, stream->codecpar);
    if (ret < 0)
        return base::error("avcodec_parameters_to_context failed");
    
    avctx_->pkt_timebase = stream->time_base;
    
    const AVCodec *codec = avcodec_find_decoder(avctx_->codec_id);
    if (!codec) {
        return base::error("No codec could be found with name:{}", avcodec_get_name(avctx_->codec_id));
    }
    
    avctx_->codec_id = codec->id;
    
//    if (fast)
//        avctx_->flags2 |= AV_CODEC_FLAG2_FAST;
    
//    AVDictionary *codec_opts = nullptr;
    AVDictionary *opts = nullptr;
//
//    ret = filter_codec_opts(codec_opts, avctx_->codec_id, fmt_ctx,
//                            stream, codec, &opts);
//    if (ret < 0)
//        goto fail;
//
//    if (!av_dict_get(opts, "threads", NULL, 0))
//        av_dict_set(&opts, "threads", "auto", 0);
//    if (stream_lowres) // set down scale level, for downgrade
//        av_dict_set_int(&opts, "lowres", stream_lowres, 0);
    
    
//    // hardware if needed
//    if (avctx_->codec_type == AVMEDIA_TYPE_VIDEO) {
//        ret = create_hwaccel(&avctx_->hw_device_ctx);
//        if (ret < 0)
//            return std::nullopt;
//    }
    
    if ((ret = avcodec_open2(avctx_, codec, &opts)) < 0) {
        return base::error("avcodec_open2 faild");
    }
    
    
    if (const AVDictionaryEntry *t = av_dict_get(opts, "", NULL, AV_DICT_IGNORE_SUFFIX); t) {
        return base::error("Option {} not found.", t->key);
    }
    
//    is->eof = 0;
//    ic->streams[stream_index]->discard = AVDISCARD_DEFAULT;
    
    const auto error = this->openConcreteStream(this->avctx_, fmt_ctx, stream);
    if (error.has_value()) {
        return error;
    }
    
    avctx_guard.dismiss();
    return std::nullopt;
}

AVCodecContext *BaseDecoder::getCodecCtx() {
    return this->avctx_;
}


//void BaseDecoder::pauseDecode() {
//    if (!this->is_decode_looping_) {
//        return;
//    }
//    
//    this->decodeCancelSource_.requestCancellation();
//    this->decodeCancelSource_ = {};
//    this->is_decode_looping_ = false;
//}

}

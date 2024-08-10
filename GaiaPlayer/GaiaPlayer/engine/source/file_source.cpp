//
//  file_source.cpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/7/5
//  
//
    

#include "file_source.hpp"

#include "base/error/error_str.hpp"
#include "base/log/log.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/frame.h>
#include <libavutil/time.h>
#include <libavutil/imgutils.h>
#include <libavutil/dict.h>

}

#include <folly/ScopeGuard.h>
#include <folly/Format.h>

#include <optional>
#include <format>


namespace gaia::engine {

folly::Expected<AVFormatContext *, std::string> FileSource::onAttach() {
    auto filename = av_strdup(this->file_.c_str());
    
    AVFormatContext *p_fmt_ctx = avformat_alloc_context();
    if (!p_fmt_ctx) {
        return base::unexpected("could not avformat_alloc_context.");
    }
    auto ctx_guard = folly::makeGuard([=](){
        avformat_free_context(p_fmt_ctx);
    });

//    // 中断回调机制。为底层I/O层提供一个处理接口，比如中止IO操作。
    p_fmt_ctx->interrupt_callback.callback = [](void *opaque){
        FileSource *src = static_cast<FileSource *>(opaque);
        return src->should_abort_ ? 1 : 0;
    };
    p_fmt_ctx->interrupt_callback.opaque = this;
    
    int err = avformat_open_input(&p_fmt_ctx, this->file_.c_str(), NULL, NULL);
    if (err < 0) {
        return base::unexpected("avformat_open_input() failed {}", av_err2str(err));
    }
    auto input_guard = folly::makeGuard([=]() mutable { avformat_close_input(&p_fmt_ctx); });
    
    
    input_guard.dismiss();
    ctx_guard.dismiss();
    
    return p_fmt_ctx;
}


}



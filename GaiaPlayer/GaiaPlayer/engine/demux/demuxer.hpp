//
//  demuxer.hpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/7/6
//  
//
    

#ifndef demuxer_hpp
#define demuxer_hpp

#include "engine/engine_env.hpp"
#include "base/async/executors.hpp"
#include "engine/decode/decoder.hpp"
#include "base/error/error_str.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/frame.h>
#include <libavutil/time.h>
#include <libavutil/imgutils.h>
}

#include <folly/Expected.h>
#include <folly/Utility.h>
#include <folly/CancellationToken.h>

#include <optional>


namespace gaia::engine {

class Demuxer: public folly::MoveOnly {
public:
    static folly::Expected<Streams, base::ErrorMsg> splitStreams(AVFormatContext *p_fmt_ctx);
    
    Demuxer(std::shared_ptr<EngineEnv> env, std::shared_ptr<gaia::base::Executors> executor, std::shared_ptr<Decoder> decoder);
    
    base::ErrorMsgOpt openStreams(AVFormatContext *p_fmt_ctx, std::shared_ptr<Streams> streams);
    
    void startDemux(int32_t serial);
    void pauseDemux();
    
private:
    folly::coro::Task<> demuxLoop(int32_t serial);
    
    struct ProduceResult {
        bool is_eof;
        bool need_delay_retry;
    };
    folly::Expected<ProduceResult, base::ErrorMsg> produceOnce();
    
    std::shared_ptr<EngineEnv> env_;
    std::shared_ptr<gaia::base::Executors> executor_;
    std::shared_ptr<Decoder> decoder_;
    
    int32_t start_demux_serial_ = {};
    bool is_eof_;
    bool is_demux_looping_ = false;
    folly::CancellationSource demux_cancel_source_;
    
    
};

}

#endif /* demuxer_hpp */

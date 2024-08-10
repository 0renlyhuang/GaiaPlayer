//
//  video_unifier.hpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/8/1
//  
//
    

#ifndef video_unifier_hpp
#define video_unifier_hpp

#include "engine/engine_env.hpp"
#include "base/error/error_str.hpp"
#include "common/frame.hpp"
#include "engine/destination/sdl_dst.hpp"



#include <folly/Expected.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>

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

#include <folly/Expected.h>
#include <optional>

namespace gaia::engine {
    
class VideoUnifier {

public:
    VideoUnifier(std::shared_ptr<EngineEnv> env, std::shared_ptr<SDLDst> sdl_dst);
    
    std::optional<base::ErrorMsg> recofigIfNeeded(FramePtr frame);
    
    AVFilterContext *filter_src_;
    AVFilterContext *filter_sink_;
    
private:
    int configureVideoFiltersGraph(AVFilterGraph *graph, const char *vfilters, AVFrame *frame);
    
    std::shared_ptr<EngineEnv> env_;
    std::shared_ptr<SDLDst> sdl_dst_;
    
    AVFilterGraph *graph_;
    
    int last_w_ = 0;
    int last_h_ = 0;
    enum AVPixelFormat last_format_ = AVPixelFormat(-2);
    int last_serial_ = -1;
    int last_vfilter_idx_ = 0;
};

}

#endif /* video_unifier_hpp */

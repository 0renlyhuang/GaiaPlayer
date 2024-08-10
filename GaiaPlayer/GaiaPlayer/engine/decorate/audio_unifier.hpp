//
//  audio_unifier.hpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/7/23
//  
//
    

#ifndef audio_unifier_hpp
#define audio_unifier_hpp

#include "base/error/error_str.hpp"
#include "common/audio/audio_params.h"

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

namespace gaia::engine {
    


class AudioUnifier {

public:
    struct ConfigureAudioFiltersResult {
        AVFilterContext *filter_src;
        AVFilterContext *filter_sink;
    };
    
    AudioUnifier() = default;

    folly::Expected<bool, base::ErrorMsg> openFilter(AVCodecContext *avctx);
    
    AVFilterContext *filter_src_;
    AVFilterContext *filter_sink_;
    
private:

    folly::Expected<ConfigureAudioFiltersResult, base::ErrorMsg> ConfigureAudioFilters(std::optional<AudioParams *> target_to_align);
    
    AVFilterGraph *graph_;
    AudioParams filter_src_config_ = {};
    

};

}

#endif /* audio_unifier_hpp */

//
//  audio_params.h
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/7/25
//  
//
    

#ifndef audio_params_h
#define audio_params_h

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


namespace gaia::engine {

struct AudioParams {
    int freq;
    AVChannelLayout ch_layout;
    enum AVSampleFormat fmt;
    int frame_size;
    int bytes_per_sec;
    uint32_t audio_buf_size;
};

}

#endif /* audio_params_h */

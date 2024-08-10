//
//  graph_helper.hpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/8/1
//  
//
    

#ifndef graph_helper_hpp
#define graph_helper_hpp

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
    
class GraphHelper {
public:
    static int ConfigureFiltergraph(AVFilterGraph *graph, const char *filtergraph,
                                           AVFilterContext *source_ctx, AVFilterContext *sink_ctx);
};

}
#endif /* graph_helper_hpp */

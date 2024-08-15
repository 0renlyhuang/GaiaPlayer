//
//  video_decoder.hpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/7/27
//  
//
    

#ifndef video_decoder_hpp
#define video_decoder_hpp

#include "engine/engine_env.hpp"
#include "engine/decode/base_decoder.hpp"
#include "engine/decode/decoded_content.hpp"
#include "engine/decorate/video_unifier.hpp"
#include "common/event/event_center.hpp"
#include "common/probe/media_probe.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/frame.h>
#include <libavutil/time.h>
#include <libavutil/imgutils.h>
}

namespace gaia::engine {

class VideoDecoder: public BaseDecoder {
    friend class ConsumeDataSource;
public:
    VideoDecoder(std::shared_ptr<EngineEnv> env, std::shared_ptr<VideoUnifier> video_unifier, std::shared_ptr<EventCenter> event_center, std::shared_ptr<MediaProbe> media_probe);
    ~VideoDecoder() override = default;
    

    std::optional<base::ErrorMsg> openConcreteStream(AVCodecContext *avctx, AVFormatContext *fmt_ctx, AVStream *stream) override;
    
    base::ErrorMsgOpt decode(PacketPtr pkt) override;
    
    bool isCacheEnough() override;
    bool isCacheInNeed() override;
    std::optional<base::TimeUnit> getDurationInQueue();
    
private:
    void updateLastFrameDuration(DecodedFramePtr last_frame, DecodedFramePtr curr_frame);
    base::ErrorMsgOpt filterFrame(FramePtr frame, PacketPtr pkt);
    
    
    
    std::shared_ptr<EngineEnv> env_;
    std::shared_ptr<VideoUnifier> video_unifier_;
    std::shared_ptr<EventCenter> event_center_;
    std::shared_ptr<MediaProbe> media_probe_;
    
    std::optional<DecodedFramePtr> last_decoded_frame_;
    std::queue<DecodedFramePtr> queue_;
};

}

#endif /* video_decoder_hpp */

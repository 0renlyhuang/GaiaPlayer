//
//  consume_data_source.hpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/8/6
//  
//
    

#ifndef consume_data_source_hpp
#define consume_data_source_hpp

#include "engine/decode/audio/audio_decoder.hpp"
#include "engine/decode/video/video_decoder.hpp"
#include <folly/concurrency/UnboundedQueue.h>

namespace gaia::engine {

class ConsumeDataSource {
public:
    ConsumeDataSource(std::shared_ptr<AudioDecoder> audio_decoder, std::shared_ptr<VideoDecoder> video_decoder);
    
    folly::USPSCQueue<DecodedFrame, false>& getAudioQueueRef() const;
    std::queue<DecodedFramePtr>& getVideoQueueRef() const;
    
private:
    std::shared_ptr<AudioDecoder> audio_decoder_;
    std::shared_ptr<VideoDecoder> video_decoder_;
    
};

}


#endif /* consume_data_source_hpp */

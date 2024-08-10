//
//  consume_data_source.cpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/8/6
//  
//
    

#include "consume_data_source.hpp"

namespace gaia::engine {

ConsumeDataSource::ConsumeDataSource(std::shared_ptr<AudioDecoder> audio_decoder, std::shared_ptr<VideoDecoder> video_decoder): audio_decoder_(audio_decoder), video_decoder_(video_decoder) {
    
}


folly::USPSCQueue<DecodedFrame, false>& ConsumeDataSource::getAudioQueueRef() const {
    return this->audio_decoder_->queue_;
}

std::queue<DecodedFramePtr>& ConsumeDataSource::getVideoQueueRef() const {
    return this->video_decoder_->queue_;
}


}

//
//  audio_queue.h
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/7/26
//  
//
    

#ifndef audio_queue_h
#define audio_queue_h

#include "common/audio/audio_buf.h"

#include <folly/ProducerConsumerQueue.h>


namespace gaia::engine {



class AudioQueue {
public:
    AudioQueue(): queue_(folly::ProducerConsumerQueue<AudioBufPtr>(100000)) {}
    
//    bool push(AudioBufPtr buf) {
//        return this->queue_.write(buf);
//    }
//    
//    std::shared_ptr<BufView> read(size_t sz) {
//        
//    }
//    
    
//private:
    folly::ProducerConsumerQueue<AudioBufPtr> queue_;
//    std::atomic<size>
};

}


#endif /* audio_queue_h */

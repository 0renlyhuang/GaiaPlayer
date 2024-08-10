//
//  engine.hpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/7/5
//  
//
    

#ifndef engine_hpp
#define engine_hpp

#include "engine/source/file_source.hpp"
#include "base/async/executors.hpp"
#include "base/buf/buf.hpp"
#include "engine/destination/sdl_dst.hpp"
#include "engine/engine_di.h"
#include "engine/consume/sdl_video_consumer.hpp"

#include <string>
#include <chrono>
#include <memory>

#include <folly/Utility.h>
#include <folly/experimental/coro/Task.h>
#include <boost/di/extension/providers/runtime_provider.hpp>



namespace gaia::engine {

class Engine: public folly::MoveOnly {
public:
    Engine(std::shared_ptr<gaia::base::Executors> executors);
    
    void play(std::string file);
    
    void seekAfter(std::chrono::milliseconds duration);
    
    void loopOnce();
    void loop();
    
private:
    void playImpl(std::string file);
    
    std::unique_ptr<FileSource> source_;
    std::shared_ptr<SDLDst> dst_;
    std::shared_ptr<gaia::base::Executors> executors_;
    std::shared_ptr<SDLVideoConsumer> sdl_video_consumer_;
    
    std::shared_ptr<EngineInjector> injector_ = nullptr;
};

}

#endif /* engine_hpp */

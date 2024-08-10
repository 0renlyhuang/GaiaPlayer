//
//  main.cpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/6/25.
//
 
#include "base/async/executors.hpp"
#include "engine/engine.hpp"
#include "base/log/log.hpp"

#include <folly/experimental/coro/Task.h>
#include <folly/experimental/coro/BlockingWait.h>
#include <folly/futures/Future.h>
#include <folly/executors/GlobalExecutor.h>
#include <folly/init/Init.h>
#include <folly/executors/CPUThreadPoolExecutor.h>
#include <folly/io/async/EventBase.h>

#include <string>

int main (int argc, char** argv) {
    gaia::base::set_up_log();
    av_log_set_level(AV_LOG_DEBUG);
    
    const auto ui_eb = std::make_shared<folly::EventBase>();
    const auto executors = std::make_shared<gaia::base::Executors>(ui_eb);
    
    auto engine = gaia::engine::Engine(executors);
    
    std::string filepath = "/Volumes/external_disk/code/GaiaPlayer/1.mp4";
    
    engine.play(std::move(filepath));
    
    engine.loop();
    
    ui_eb->loopForever();

    return 0;
}

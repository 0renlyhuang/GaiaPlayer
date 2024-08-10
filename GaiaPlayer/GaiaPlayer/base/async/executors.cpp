//
//  executors.cpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/7/2
//  
//
    

#include "executors.hpp"

#include <folly/executors/thread_factory/NamedThreadFactory.h>

namespace gaia::base {

Executors::Executors(std::shared_ptr<folly::IOExecutor> ui_executor)
    : ui_executor_(ui_executor) {
        logic_executor_ = std::make_shared<folly::CPUThreadPoolExecutor>(
                                                                         1,
                                                                         std::make_shared<folly::NamedThreadFactory>("logic_executor_")
                                                                         );
        
        background_executor_ = std::make_shared<folly::CPUThreadPoolExecutor>(
                                                                              1,
                                                                              std::make_shared<folly::NamedThreadFactory>("background_executor_")
                                                                              );
        gpu_executor_ = std::make_shared<folly::CPUThreadPoolExecutor>(
                                                                       1,
                                                                       std::make_shared<folly::NamedThreadFactory>("gpu_executor_")
                                                                       );
        
        file_executor_ = std::make_shared<folly::CPUThreadPoolExecutor>(
                                                                        1,
                                                                        std::make_shared<folly::NamedThreadFactory>("file_executor_")
                                                                        );
}

}

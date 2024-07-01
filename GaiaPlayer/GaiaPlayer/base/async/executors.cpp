//
//  executors.cpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/7/2
//  
//
    

#include "executors.hpp"

namespace gaia::base {

Executors::Executors(std::shared_ptr<folly::IOExecutor> ui_executor)
    : ui_executor_(ui_executor) {
        logic_executor_ = std::make_shared<folly::CPUThreadPoolExecutor>(1);
        background_executor_ = std::make_shared<folly::CPUThreadPoolExecutor>(1);
        gpu_executor_ = std::make_shared<folly::CPUThreadPoolExecutor>(1);
        file_executor_ = std::make_shared<folly::CPUThreadPoolExecutor>(1);
}

}

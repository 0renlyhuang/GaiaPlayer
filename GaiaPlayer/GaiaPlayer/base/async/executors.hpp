//
//  executors.hpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/7/2
//  
//
    

#ifndef executors_hpp
#define executors_hpp

#include <folly/io/async/EventBase.h>
#include <folly/executors/CPUThreadPoolExecutor.h>
#include <folly/Utility.h>

#include <memory>

namespace gaia::base {

class Executors: public folly::MoveOnly {
public:
    Executors(std::shared_ptr<folly::IOExecutor> ui_executor);
    
//    mainThreadExecutor = std::make_shared<folly::CPUThreadPoolExecutor>(1);
    
    std::shared_ptr<folly::IOExecutor> UI() const { return this->ui_executor_; }
    
    std::shared_ptr<folly::CPUThreadPoolExecutor> Logic() const { return this->logic_executor_; }
    
    
private:
    std::shared_ptr<folly::IOExecutor> ui_executor_;
    std::shared_ptr<folly::CPUThreadPoolExecutor> logic_executor_;
    std::shared_ptr<folly::CPUThreadPoolExecutor> background_executor_;
    std::shared_ptr<folly::CPUThreadPoolExecutor> gpu_executor_;
    std::shared_ptr<folly::CPUThreadPoolExecutor> file_executor_;
    
};

}


#endif /* executors_hpp */

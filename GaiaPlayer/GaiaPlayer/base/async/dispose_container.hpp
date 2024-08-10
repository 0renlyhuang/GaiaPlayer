//
//  dispose_container.hpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/7/3
//  
//
    

#ifndef dispose_container_hpp
#define dispose_container_hpp

#include <folly/Utility.h>
#include <folly/CancellationToken.h>
#include <folly/futures/Future.h>

namespace gaia::base {

class DisposeDefer {
public:
    DisposeDefer(folly::CancellationToken cancel_token, std::function<void()> dispose_func): cancel_token_(cancel_token), dispose_func_(std::move(dispose_func)) {
        
    }
    
    ~DisposeDefer() {
        std::invoke(dispose_func_);
    }
    
    
    
private:
    folly::CancellationToken cancel_token_;
    std::function<void()> dispose_func_;
};


class DisposeContainer: public folly::MoveOnly {
public:

    
private:
    struct State {
        friend class DisposeContainer;
    private:
        std::mutex mtx_;
        std::vector<folly::Future<void>> futures_;
    };
    
    
    friend struct State;
public:
    DisposeContainer() = default;
    
    void dispose() {
        if (!cancel_src_.isCancellationRequested()) {
            cancel_src_.requestCancellation();
        }
        
        auto allFutures = folly::collectAll(state_->futures_.begin(), state_->futures_.end());
        allFutures.wait();
    }
    
    DisposeDefer getAutoWaitDisposeDefer() {
        
    }
    
    DisposeDefer getDisposeDefer() {
        
    }
    

    
    folly::CancellationSource cancel_src_;
    std::shared_ptr<State> state_;
};

}



#endif /* dispose_container_hpp */

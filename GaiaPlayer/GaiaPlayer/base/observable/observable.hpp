//
//  observable.hpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/8/12
//  
//
    

#ifndef observable_hpp
#define observable_hpp

#include <folly/Utility.h>

#include <memory>
#include <functional>
#include <unordered_set>

namespace gaia::base {

template<typename T>
class Observable;

template<typename T>
class Observer;

class SubscribeCancellation {
public:
    virtual ~SubscribeCancellation() = default;
    virtual void cancel() = 0;
};

using SubscribeHandlerPtr = std::shared_ptr<SubscribeCancellation>;


template<typename T>
class Snapshot {
public:
 const T& operator*() const { return *get(); }

 const T* operator->() const { return get(); }

 const T* get() const { return data_.get(); }

 std::shared_ptr<const T> getShared() const& { return data_; }

 std::shared_ptr<const T> getShared() && { return std::move(data_); }
    
private:
    friend Observable<T>;

    Snapshot(
        std::shared_ptr<const T> data)
        : data_(std::move(data)) {
    }
    
    std::shared_ptr<const T> data_;
};


namespace observable_detail {
template<typename T>
class Context {
public:
    Context(): mtx_v_(), value_(nullptr), mtx_(), cancellation_2_observer_() {}
    
    std::mutex mtx_v_;
    std::shared_ptr<const T> value_;
    
    std::mutex mtx_;
    std::unordered_map<SubscribeCancellation *, std::shared_ptr<Observer<T>> > cancellation_2_observer_;
    
private:
    
};

template<typename T>
class SubscribeCancellationImpl: public SubscribeCancellation, public folly::MoveOnly {
public:
    SubscribeCancellationImpl(std::shared_ptr<observable_detail::Context<T>> ctx): is_cancel_(false), ctx_(ctx) {}
    
    ~SubscribeCancellationImpl() override {
        if (!this->is_cancel_) {
            this->cancel();
        }
    };
    
    void cancel() override {
        if (this->is_cancel_) {
            return;
        }
        
        {
            const auto lck = std::unique_lock(this->ctx_->mtx_);
            
            this->ctx_->cancellation_2_observer_.erase(this);
        }
        
        this->is_cancel_ = true;
    }
    
private:
    bool is_cancel_;
    
    std::shared_ptr<observable_detail::Context<T>> ctx_;
};


template<typename T>
class ClosureObserver: public Observer<T> {
public:
    ClosureObserver(std::function<void(Snapshot<T> snapshot)> func): func_(std::move(func)) {}
    ~ClosureObserver() override = default;
    
    void operator()(Snapshot<T> snapshot) override {
        std::invoke(this->func_, std::move(snapshot));
    }
    
private:
    std::function<void(Snapshot<T> snapshot)> func_;
};


}


template<typename T>
class Observer {
public:
    virtual ~Observer() = default;
    
    virtual void operator()(Snapshot<T> snapshot) = 0;
};



template<typename T>
class Observable {
public:
    Observable(): ctx_(std::make_shared<observable_detail::Context<T>>()) {
        
    }
    
    Snapshot<T> getSnapshot() const {
        decltype(this->ctx_->value) v = nullptr;
        
        {
            const auto lck = std::unique_lock(this->ctx_->mtx_v_);
            v = this->ctx_->value;
        }
        
        return Snapshot<T>(v);
    }
    
    void setValue(T value) {
        setValue(std::make_shared<const T>(std::move(value)));
    }
    
    void setValue(std::shared_ptr<const T> value) {
        {
            const auto lck = std::unique_lock(this->ctx_->mtx_v_);
            this->ctx_->value_ = value;
        }
        
        {
            const auto lck = std::unique_lock(this->ctx_->mtx_);
            for (const auto& cancellation_and_observer: this->ctx_->cancellation_2_observer_) {
                const auto observer = cancellation_and_observer.second;
                observer->operator()(Snapshot<T>(value));
            }
        }
    }
    
    SubscribeHandlerPtr subscribe(std::function<void(Snapshot<T>)> callback) {
        // lock
        // add callback to list
        const auto cancellation = std::make_shared<observable_detail::SubscribeCancellationImpl<T>>(this->ctx_);
        const auto observer = std::make_shared<observable_detail::ClosureObserver<T>>(std::move(callback));
        
        {
            const auto lck = std::unique_lock(this->ctx_->mtx_);
            SubscribeCancellation *base_cancellation = cancellation.get();
            this->ctx_->cancellation_2_observer_.emplace(base_cancellation, observer);
        }
        
        return cancellation;
    }
    
    
private:
    std::shared_ptr<observable_detail::Context<T>> ctx_;
};

}

#endif /* observable_hpp */

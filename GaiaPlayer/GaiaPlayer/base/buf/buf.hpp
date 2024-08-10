//
//  buf.hpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/8/4
//  
//
    

#ifndef buf_hpp
#define buf_hpp

#include <folly/Utility.h>

#include <memory>
#include <stddef.h>

namespace gaia::base {

struct Buf: public folly::MoveOnly {
    std::byte *raw;
    size_t sz;

    
    Buf(): raw(nullptr), sz(0), capacity_(0) {
        
    }
    
    Buf(size_t a_capacity): sz(0), capacity_(a_capacity) {
        this->raw = new std::byte[a_capacity];
    }
    
    ~Buf() {
        if (this->raw) {
            delete []this->raw;
        }
    }
    
    Buf(Buf&&) noexcept = default;
    Buf& operator=(Buf&&) noexcept = default;
    
    size_t rest() const {
        return this->capacity_ - this->sz;
    }
    
    size_t capacity() const {
        return this->capacity_;
    }
    
    void expandTo(size_t new_capacity) {
        auto buf = base::Buf(new_capacity);
        
        if (this->sz > 0) {
            memmove(buf.raw, this->raw, this->sz);
        }
        
        *this = std::move(buf);
    }
    
    void expandMore(size_t more_capacity) {
        this->expandTo(this->capacity_ + more_capacity);
    }
    
private:
    size_t capacity_;
};

}

#endif /* buf_hpp */

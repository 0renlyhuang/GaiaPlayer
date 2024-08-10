//
//  audio_buf.h
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/7/26
//  
//
    

#ifndef audio_buf_h
#define audio_buf_h

#include <memory>
#include <any>
#include <folly/Utility.h>

namespace gaia::engine {

template <typename Context>
struct Buf: public folly::MoveOnly {
    size_t size;
    std::byte* buf;
    Context ctx;
    
    Buf(size_t a_size, Context&& a_ctx): size(a_size), ctx(std::forward<Context&&>(ctx)) {
        this->buf = new std::byte[a_size];
    }
    
    ~Buf() override {
        delete[] this->buf;
    }
};

template <typename Context>
using BufPtr = std::shared_ptr<Buf<Context>>;

template <typename Context>
BufPtr<Context> makeBuf(size_t size, Context&& ctx) {
    return std::make_shared<Buf<Context>>(size, std::forward<Context&&>(ctx));
}


struct AuidoBufCtx {
    
};

using AudioBuf = Buf<AuidoBufCtx>;
using AuidoBufPtr = BufPtr<AuidoBufCtx>;

}


#endif /* audio_buf_h */

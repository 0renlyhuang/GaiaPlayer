//
//  buf_view.hpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/8/5
//  
//
    

#ifndef buf_view_hpp
#define buf_view_hpp

#include <folly/Utility.h>

#include <memory>
#include <stddef.h>

namespace gaia::base {

struct BufView {
    const std::byte *raw;
    const size_t sz;
    
    BufView(std::byte *a_raw, size_t a_sz): raw(a_raw), sz(a_sz) {
    }
    
    ~BufView() {
    }
};


}

#endif /* buf_view_hpp */

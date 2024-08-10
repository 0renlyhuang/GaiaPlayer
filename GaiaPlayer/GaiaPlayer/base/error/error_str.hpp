//
//  error_str.hpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/7/24
//  
//
    

#ifndef error_str_hpp
#define error_str_hpp

#include <stdio.h>
#include <folly/Format.h>
#include <folly/Expected.h>

namespace gaia::base {

using ErrorMsg = std::string;
using ErrorMsgOpt = std::optional<ErrorMsg>;
constexpr auto noError = std::nullopt;

template <class... Args>
ErrorMsg error(folly::StringPiece fmt, Args&&... args) {
    auto msg = folly::sformat(std::move(fmt), std::forward<Args>(args)...);
    assert(false);
    
    return msg;
}

template <class... Args>
folly::Unexpected<ErrorMsg> unexpected(folly::StringPiece fmt, Args&&... args) {
    return folly::makeUnexpected(error(std::move(fmt), std::forward<Args>(args)...));
}

}

#endif /* error_str_hpp */

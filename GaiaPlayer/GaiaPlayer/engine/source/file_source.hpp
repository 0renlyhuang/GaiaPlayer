//
//  file_source.hpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/7/5
//  
//
    

#ifndef file_source_hpp
#define file_source_hpp

#include "engine/engine_di_def.h"

#include <string>
#include <folly/Expected.h>
#include <boost/di.hpp>
#include <string_view>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/frame.h>
#include <libavutil/time.h>
#include <libavutil/imgutils.h>
}

namespace gaia::engine {

class ISource {
public:
    virtual ~ISource() = default;
    
    virtual folly::Expected<AVFormatContext *, std::string> onAttach() = 0;
};

class FileSource: public ISource {
public:
    BOOST_DI_INJECT(FileSource, (named = def::input_file_name) const std::string& file): file_(file) {
        printf("aa");
    }
    
    
    ~FileSource() override = default;
    
    folly::Expected<AVFormatContext *, std::string> onAttach() override;
    
private:
    std::string file_;
    bool should_abort_ = false;
    
};

}

#endif /* file_source_hpp */

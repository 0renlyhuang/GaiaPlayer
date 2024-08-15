//
//  sdl_dst.hpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/7/23
//  
//
    

#ifndef sdl_dst_hpp
#define sdl_dst_hpp

#include "common/audio/audio_params.h"
#include "base/error/error_str.hpp"
#include "base/buf/buf.hpp"
#include "common/frame.hpp"
#include "base/buf/buf_view.hpp"
#include "engine/engine_env.hpp"
#include "engine/decode/decoded_content.hpp"
#include "base/async/executors.hpp"
#include "common/perf/perf_tracker.hpp"

extern "C" {
#include <libavutil/dict.h>
#include <libavutil/opt.h>
#include <libavfilter/avfilter.h>
#include <libavutil/bprint.h>
#include <libavutil/channel_layout.h>

#include "libavutil/avstring.h"
#include "libavutil/channel_layout.h"
#include "libavutil/eval.h"
#include "libavutil/mathematics.h"
#include "libavutil/pixdesc.h"
#include "libavutil/imgutils.h"
#include "libavutil/dict.h"
#include "libavutil/fifo.h"
#include "libavutil/parseutils.h"
#include "libavutil/samplefmt.h"
#include "libavutil/time.h"
#include "libavutil/bprint.h"
#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#include "libavutil/opt.h"
#include "libavutil/tx.h"
#include "libswresample/swresample.h"

#include "libavfilter/avfilter.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
}

#include <SDL2/SDL.h>

#include <folly/Expected.h>
#include <optional>
#include <functional>
#include <memory>

namespace gaia::engine {
    
class ISDLDstProvider {
public:
    virtual ~ISDLDstProvider() = default;
    
    using FillAudioImplFunc = std::function<void(std::byte *dst, const std::byte *src, size_t sz)>;
    virtual base::ErrorMsgOpt provideAudio(std::byte *dst, size_t sz, FillAudioImplFunc fill_func_impl) = 0;
};

class SDLDst {
public:
    SDLDst();
    ~SDLDst(); // override;
    
    void loopOnce();
    
    void onAttch(std::shared_ptr<EngineEnv> env, std::shared_ptr<base::Executors> executor, std::shared_ptr<PerfTracker> perf_tracker);
    
    
    std::optional<base::ErrorMsg> launchWnd();
    void closeWnd();
    
    folly::Expected<AudioParams, std::string> openAudioDevice(AVChannelLayout *wanted_channel_layout, int wanted_sample_rate);
    
    void sdlAudioCallback(Uint8 *stream, int len);
    
    void startAudioDevice(std::weak_ptr<ISDLDstProvider> provider);    
    
    AVPixelFormat *getSupportPixFmts();
    AVColorSpace *getSupportColorSpace();
    const AudioParams& getAudioHwParamsRef();
    
    void showFrame(DecodedFramePtr frame);
    
private:
    void showFrameImpl(DecodedFramePtr decoded_frame);
    void showWnd();
    int realloc_texture(SDL_Texture **texture, Uint32 new_format, int new_width, int new_height, SDL_BlendMode blendmode, int init_texture);
    int upload_texture(SDL_Texture **tex, AVFrame *frame);
    
    
    std::weak_ptr<ISDLDstProvider> provider_;
    
    std::shared_ptr<EngineEnv> env_;
    std::shared_ptr<base::Executors> executor_;
    std::shared_ptr<PerfTracker> perf_tracker_;
    
    SDL_Window *window_ = nullptr;
    SDL_Renderer *renderer_ = nullptr;
    SDL_Texture *vid_texture_ = nullptr;
    
    SDL_RendererInfo renderer_info_ = {0};
    AVPixelFormat *support_pix_fmts_;
    SDL_AudioDeviceID device_id_;
    
    AudioParams audio_hw_params_ = {};
    
    
    int default_wnd_width_ = 640;
    int default_wnd_height_ = 480;
    bool ever_update_wnd_size_from_frame_ = false;
    
    bool ever_show_wnd_ = false;
};

}

#endif /* sdl_dst_hpp */

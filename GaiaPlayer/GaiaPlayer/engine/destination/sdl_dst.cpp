//
//  sdl_dst.cpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/7/23
//  
//
    

#include "sdl_dst.hpp"

#include "base/error/error_str.hpp"

#include <SDL2/SDL.h>
#include "base/log/log.hpp"
#include <folly/Format.h>

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
    
/* Minimum SDL audio buffer size, in samples. */
#define SDL_AUDIO_MIN_BUFFER_SIZE 512
/* Calculate actual buffer size keeping in mind not cause too frequent audio callbacks */
#define SDL_AUDIO_MAX_CALLBACKS_PER_SEC 30


static const struct TextureFormatEntry {
    enum AVPixelFormat format;
    int texture_fmt;
} sdl_texture_format_map[] = {
    { AV_PIX_FMT_RGB8,           SDL_PIXELFORMAT_RGB332 },
    { AV_PIX_FMT_RGB444,         SDL_PIXELFORMAT_RGB444 },
    { AV_PIX_FMT_RGB555,         SDL_PIXELFORMAT_RGB555 },
    { AV_PIX_FMT_BGR555,         SDL_PIXELFORMAT_BGR555 },
    { AV_PIX_FMT_RGB565,         SDL_PIXELFORMAT_RGB565 },
    { AV_PIX_FMT_BGR565,         SDL_PIXELFORMAT_BGR565 },
    { AV_PIX_FMT_RGB24,          SDL_PIXELFORMAT_RGB24 },
    { AV_PIX_FMT_BGR24,          SDL_PIXELFORMAT_BGR24 },
    { AV_PIX_FMT_0RGB32,         SDL_PIXELFORMAT_RGB888 },
    { AV_PIX_FMT_0BGR32,         SDL_PIXELFORMAT_BGR888 },
    { AV_PIX_FMT_NE(RGB0, 0BGR), SDL_PIXELFORMAT_RGBX8888 },
    { AV_PIX_FMT_NE(BGR0, 0RGB), SDL_PIXELFORMAT_BGRX8888 },
    { AV_PIX_FMT_RGB32,          SDL_PIXELFORMAT_ARGB8888 },
    { AV_PIX_FMT_RGB32_1,        SDL_PIXELFORMAT_RGBA8888 },
    { AV_PIX_FMT_BGR32,          SDL_PIXELFORMAT_ABGR8888 },
    { AV_PIX_FMT_BGR32_1,        SDL_PIXELFORMAT_BGRA8888 },
    { AV_PIX_FMT_YUV420P,        SDL_PIXELFORMAT_IYUV },
    { AV_PIX_FMT_YUYV422,        SDL_PIXELFORMAT_YUY2 },
    { AV_PIX_FMT_UYVY422,        SDL_PIXELFORMAT_UYVY },
    { AV_PIX_FMT_NONE,           SDL_PIXELFORMAT_UNKNOWN },
};


static enum AVColorSpace sdl_supported_color_spaces[] = {
    AVCOL_SPC_BT709,
    AVCOL_SPC_BT470BG,
    AVCOL_SPC_SMPTE170M,
    AVCOL_SPC_UNSPECIFIED,
};


static void sdl_audio_callback(void *opaque, Uint8 *stream, int len) {
    SDLDst *dst = static_cast<SDLDst *>(opaque);
    dst->sdlAudioCallback(stream, len);
}


SDLDst::SDLDst(): support_pix_fmts_(new AVPixelFormat[FF_ARRAY_ELEMS(sdl_texture_format_map)]) {
    
}

SDLDst::~SDLDst() {
    delete []this->support_pix_fmts_;
}

folly::Expected<AudioParams, std::string> SDLDst::openAudioDevice(AVChannelLayout *wanted_channel_layout, int wanted_sample_rate) {
    SDL_AudioSpec wanted_spec, spec;
    const char *env;
    static const int next_nb_channels[] = {0, 0, 1, 6, 2, 6, 4, 6};
    static const int next_sample_rates[] = {0, 44100, 48000, 96000, 192000};
    int next_sample_rate_idx = FF_ARRAY_ELEMS(next_sample_rates) - 1;
    int wanted_nb_channels = wanted_channel_layout->nb_channels;

    env = SDL_getenv("SDL_AUDIO_CHANNELS");
    if (env) {
        wanted_nb_channels = atoi(env);
        av_channel_layout_uninit(wanted_channel_layout);
        av_channel_layout_default(wanted_channel_layout, wanted_nb_channels);
    }
    if (wanted_channel_layout->order != AV_CHANNEL_ORDER_NATIVE) {
        av_channel_layout_uninit(wanted_channel_layout);
        av_channel_layout_default(wanted_channel_layout, wanted_nb_channels);
    }
    wanted_nb_channels = wanted_channel_layout->nb_channels;
    wanted_spec.channels = wanted_nb_channels;
    wanted_spec.freq = wanted_sample_rate;
    if (wanted_spec.freq <= 0 || wanted_spec.channels <= 0) {
        return base::unexpected("Invalid sample rate or channel count");
    }
    while (next_sample_rate_idx && next_sample_rates[next_sample_rate_idx] >= wanted_spec.freq)
        next_sample_rate_idx--;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.silence = 0;
    wanted_spec.samples = FFMAX(SDL_AUDIO_MIN_BUFFER_SIZE, 2 << av_log2(wanted_spec.freq / SDL_AUDIO_MAX_CALLBACKS_PER_SEC));
    wanted_spec.callback = sdl_audio_callback;
    wanted_spec.userdata = this;
    while (!(this->device_id_ = SDL_OpenAudioDevice(NULL, 0, &wanted_spec, &spec, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE))) {
        av_log(NULL, AV_LOG_WARNING, "SDL_OpenAudio (%d channels, %d Hz): %s\n",
               wanted_spec.channels, wanted_spec.freq, SDL_GetError());
        wanted_spec.channels = next_nb_channels[FFMIN(7, wanted_spec.channels)];
        if (!wanted_spec.channels) {
            wanted_spec.freq = next_sample_rates[next_sample_rate_idx--];
            wanted_spec.channels = wanted_nb_channels;
            if (!wanted_spec.freq) {
                return base::unexpected("No more combinations to try, audio open failed\n");
            }
        }
        av_channel_layout_default(wanted_channel_layout, wanted_spec.channels);
    }
    if (spec.format != AUDIO_S16SYS) {
        return base::unexpected("SDL advised audio format %d is not supported", spec.format);
    }
    if (spec.channels != wanted_spec.channels) {
        av_channel_layout_uninit(wanted_channel_layout);
        av_channel_layout_default(wanted_channel_layout, spec.channels);
        if (wanted_channel_layout->order != AV_CHANNEL_ORDER_NATIVE) {
            return base::unexpected("SDL advised channel count %d is not supported!\n", spec.channels);
        }
    }

    AudioParams audio_hw_params = {};
    audio_hw_params.fmt = AV_SAMPLE_FMT_S16;
    audio_hw_params.freq = spec.freq;
    if (av_channel_layout_copy(&audio_hw_params.ch_layout, wanted_channel_layout) < 0)
        return base::unexpected("av_channel_layout_copy faild");
    audio_hw_params.frame_size = av_samples_get_buffer_size(NULL, audio_hw_params.ch_layout.nb_channels, 1, audio_hw_params.fmt, 1);
    audio_hw_params.bytes_per_sec = av_samples_get_buffer_size(NULL, audio_hw_params.ch_layout.nb_channels, audio_hw_params.freq, audio_hw_params.fmt, 1);
    if (audio_hw_params.bytes_per_sec <= 0 || audio_hw_params.frame_size <= 0) {
        return base::unexpected("av_samples_get_buffer_size failed\n");
    }
    
    audio_hw_params.audio_buf_size = spec.size;
    
    this->audio_hw_params_ = audio_hw_params;
    return audio_hw_params;
}

void SDLDst::sdlAudioCallback(Uint8 *stream, int len) {
    // called from random thread
    
    memset(stream, 0, len);
    
    const auto provider = this->provider_.lock();
    if (!provider) {
        return;
    }
    
    ISDLDstProvider::FillAudioImplFunc fill_impl = [](std::byte *dst, const std::byte *src, size_t sz){
        constexpr auto audio_volume = SDL_MIX_MAXVOLUME / 3;
        SDL_MixAudioFormat(reinterpret_cast<Uint8 *>(dst), reinterpret_cast<const Uint8 *>(src), AUDIO_S16SYS, static_cast<Uint32>(sz), audio_volume);
    };
    
    const auto error = provider->provideAudio(reinterpret_cast<std::byte *>(stream), static_cast<size_t>(len), fill_impl);
    
}

void SDLDst::startAudioDevice(std::weak_ptr<ISDLDstProvider> provider) {
    if (this->device_id_ > 0) {
        this->provider_ = provider;
        SDL_PauseAudioDevice(this->device_id_, 0);
    }
}



AVPixelFormat *SDLDst::getSupportPixFmts() {
    return this->support_pix_fmts_;
}


AVColorSpace *SDLDst::getSupportColorSpace() {
    return sdl_supported_color_spaces;
}



std::optional<base::ErrorMsg>  SDLDst::launchWnd() {
    if (SDL_Init (SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        return base::error("Could not initialize SDL - {}", SDL_GetError());
    }
    
    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_DEBUG);
    
    
    SDL_EventState(SDL_SYSWMEVENT, SDL_IGNORE);
    SDL_EventState(SDL_USEREVENT, SDL_IGNORE);
    
    int flags = SDL_WINDOW_HIDDEN;
    constexpr bool borderless = false;
    if (borderless)
        flags |= SDL_WINDOW_BORDERLESS;
    else
        flags |= SDL_WINDOW_RESIZABLE;
    
    
#ifdef SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR
        SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
#endif
    
    
//    bool enable_vulkan = false;
//    constexpr bool hwaccel = false;
//    
//    if (hwaccel && !enable_vulkan) {
//        enable_vulkan = true;
//    }
//    
//    if (enable_vulkan) {
//        vk_renderer = vk_get_renderer();
//        if (vk_renderer) {
//#if SDL_VERSION_ATLEAST(2, 0, 6)
//            flags |= SDL_WINDOW_VULKAN;
//#endif
//        } else {
//            av_log(NULL, AV_LOG_WARNING, "Doesn't support vulkan renderer, fallback to SDL renderer\n");
//            enable_vulkan = 0;
//        }
//    }
    
//    this->window_ = SDL_CreateWindow("simple ffplayer",
//                              SDL_WINDOWPOS_UNDEFINED,// 不关心窗口X坐标
//                              SDL_WINDOWPOS_UNDEFINED,// 不关心窗口Y坐标
//                     this->default_wnd_width_,
//                     this->default_wnd_height_,
//                              SDL_WINDOW_OPENGL
//                              );
    this->window_ = SDL_CreateWindow("Gaia Player", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, this->default_wnd_width_, this->default_wnd_height_, SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    if (!this->window_) {
        return base::error("Failed to create window: %s", SDL_GetError());
    }
    auto wnd_guard = folly::makeGuard([this]{ SDL_DestroyWindow(this->window_); });
    
//    if (vk_renderer) {
//        AVDictionary *dict = NULL;
//
//        if (vulkan_params)
//            av_dict_parse_string(&dict, vulkan_params, "=", ":", 0);
//        ret = vk_renderer_create(vk_renderer, window, dict);
//        av_dict_free(&dict);
//        if (ret < 0) {
//            av_log(NULL, AV_LOG_FATAL, "Failed to create vulkan renderer, %s\n", av_err2str(ret));
//            do_exit(NULL);
//        }
//    } else {
    
    this->renderer_ = SDL_CreateRenderer(this->window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!this->renderer_) {
        LOG(WARNING) << folly::sformat("Failed to initialize a hardware accelerated renderer: {}", SDL_GetError());
        this->renderer_ = SDL_CreateRenderer(this->window_, -1, 0);
    }
    
    if (!this->renderer_) {
        return base::error("Failed to create renderer", SDL_GetError());
    }
    auto renderer_guard = folly::makeGuard([this]{ SDL_DestroyRenderer(this->renderer_); });
    
    
    
    if (SDL_GetRendererInfo(this->renderer_, &this->renderer_info_) < 0) {
        return base::error("Failed to SDL_GetRendererInfo {}", SDL_GetError());
    }
    
    if (!this->renderer_info_.num_texture_formats) {
        return base::error("Failed as no texture formats supported");
    }

    int nb_pix_fmts = 0;
    for (int i = 0; i < this->renderer_info_.num_texture_formats; i++) {
        for (int j = 0; j < FF_ARRAY_ELEMS(sdl_texture_format_map) - 1; j++) {
            if (this->renderer_info_.texture_formats[i] == sdl_texture_format_map[j].texture_fmt) {
                this->support_pix_fmts_[nb_pix_fmts++] = sdl_texture_format_map[j].format;
                break;
            }
        }
    }
    this->support_pix_fmts_[nb_pix_fmts] = AV_PIX_FMT_NONE;

    
    renderer_guard.dismiss();
    wnd_guard.dismiss();
    return std::nullopt;
}

void SDLDst::closeWnd() {
    if (this->renderer_)
        SDL_DestroyRenderer(this->renderer_);
    
    if (this->window_)
        SDL_DestroyWindow(this->window_);
}


const AudioParams& SDLDst::getAudioHwParamsRef() {
    return this->audio_hw_params_;
}

void SDLDst::onAttch(std::shared_ptr<EngineEnv> env, std::shared_ptr<base::Executors> executor) {
    this->env_ = env;
    this->executor_ = executor;
}


static void get_sdl_pix_fmt_and_blendmode(int format, Uint32 *sdl_pix_fmt, SDL_BlendMode *sdl_blendmode)
{
    int i;
    *sdl_blendmode = SDL_BLENDMODE_NONE;
    *sdl_pix_fmt = SDL_PIXELFORMAT_UNKNOWN;
    if (format == AV_PIX_FMT_RGB32   ||
        format == AV_PIX_FMT_RGB32_1 ||
        format == AV_PIX_FMT_BGR32   ||
        format == AV_PIX_FMT_BGR32_1)
        *sdl_blendmode = SDL_BLENDMODE_BLEND;
    for (i = 0; i < FF_ARRAY_ELEMS(sdl_texture_format_map) - 1; i++) {
        if (format == sdl_texture_format_map[i].format) {
            *sdl_pix_fmt = sdl_texture_format_map[i].texture_fmt;
            return;
        }
    }
}

static void calculate_display_rect(SDL_Rect *rect,
                                   int scr_xleft, int scr_ytop, int scr_width, int scr_height,
                                   int pic_width, int pic_height, AVRational pic_sar)
{
    AVRational aspect_ratio = pic_sar;
    int64_t width, height, x, y;

    if (av_cmp_q(aspect_ratio, av_make_q(0, 1)) <= 0)
        aspect_ratio = av_make_q(1, 1);

    aspect_ratio = av_mul_q(aspect_ratio, av_make_q(pic_width, pic_height));

    /* XXX: we suppose the screen has a 1.0 pixel ratio */
    height = scr_height;
    width = av_rescale(height, aspect_ratio.num, aspect_ratio.den) & ~1;
    if (width > scr_width) {
        width = scr_width;
        height = av_rescale(width, aspect_ratio.den, aspect_ratio.num) & ~1;
    }
    x = (scr_width - width) / 2;
    y = (scr_height - height) / 2;
    rect->x = scr_xleft + x;
    rect->y = scr_ytop  + y;
    rect->w = FFMAX((int)width,  1);
    rect->h = FFMAX((int)height, 1);
}

static void set_sdl_yuv_conversion_mode(AVFrame *frame)
{
#if SDL_VERSION_ATLEAST(2,0,8)
    SDL_YUV_CONVERSION_MODE mode = SDL_YUV_CONVERSION_AUTOMATIC;
    if (frame && (frame->format == AV_PIX_FMT_YUV420P || frame->format == AV_PIX_FMT_YUYV422 || frame->format == AV_PIX_FMT_UYVY422)) {
        if (frame->color_range == AVCOL_RANGE_JPEG)
            mode = SDL_YUV_CONVERSION_JPEG;
        else if (frame->colorspace == AVCOL_SPC_BT709)
            mode = SDL_YUV_CONVERSION_BT709;
        else if (frame->colorspace == AVCOL_SPC_BT470BG || frame->colorspace == AVCOL_SPC_SMPTE170M)
            mode = SDL_YUV_CONVERSION_BT601;
    }
    SDL_SetYUVConversionMode(mode); /* FIXME: no support for linear transfer */
#endif
}

int SDLDst::realloc_texture(SDL_Texture **texture, Uint32 new_format, int new_width, int new_height, SDL_BlendMode blendmode, int init_texture)
{
    Uint32 format;
    int access, w, h;
    if (!*texture || SDL_QueryTexture(*texture, &format, &access, &w, &h) < 0 || new_width != w || new_height != h || new_format != format) {
        void *pixels;
        int pitch;
        if (*texture)
            SDL_DestroyTexture(*texture);
        if (!(*texture = SDL_CreateTexture(this->renderer_, new_format, SDL_TEXTUREACCESS_STREAMING, new_width, new_height)))
            return -1;
        if (SDL_SetTextureBlendMode(*texture, blendmode) < 0)
            return -1;
        if (init_texture) {
            if (SDL_LockTexture(*texture, NULL, &pixels, &pitch) < 0)
                return -1;
            memset(pixels, 0, pitch * new_height);
            SDL_UnlockTexture(*texture);
        }
        av_log(NULL, AV_LOG_VERBOSE, "Created %dx%d texture with %s.\n", new_width, new_height, SDL_GetPixelFormatName(new_format));
    }
    return 0;
}

int SDLDst::upload_texture(SDL_Texture **tex, AVFrame *frame)
{
    int ret = 0;
    Uint32 sdl_pix_fmt;
    SDL_BlendMode sdl_blendmode;
    get_sdl_pix_fmt_and_blendmode(frame->format, &sdl_pix_fmt, &sdl_blendmode);
    if (this->realloc_texture(tex, sdl_pix_fmt == SDL_PIXELFORMAT_UNKNOWN ? SDL_PIXELFORMAT_ARGB8888 : sdl_pix_fmt, frame->width, frame->height, sdl_blendmode, 0) < 0)
        return -1;
    switch (sdl_pix_fmt) {
        case SDL_PIXELFORMAT_IYUV:
            if (frame->linesize[0] > 0 && frame->linesize[1] > 0 && frame->linesize[2] > 0) {
                ret = SDL_UpdateYUVTexture(*tex, NULL, frame->data[0], frame->linesize[0],
                                                       frame->data[1], frame->linesize[1],
                                                       frame->data[2], frame->linesize[2]);
            } else if (frame->linesize[0] < 0 && frame->linesize[1] < 0 && frame->linesize[2] < 0) {
                ret = SDL_UpdateYUVTexture(*tex, NULL, frame->data[0] + frame->linesize[0] * (frame->height                    - 1), -frame->linesize[0],
                                                       frame->data[1] + frame->linesize[1] * (AV_CEIL_RSHIFT(frame->height, 1) - 1), -frame->linesize[1],
                                                       frame->data[2] + frame->linesize[2] * (AV_CEIL_RSHIFT(frame->height, 1) - 1), -frame->linesize[2]);
            } else {
                av_log(NULL, AV_LOG_ERROR, "Mixed negative and positive linesizes are not supported.\n");
                return -1;
            }
            break;
        default:
            if (frame->linesize[0] < 0) {
                ret = SDL_UpdateTexture(*tex, NULL, frame->data[0] + frame->linesize[0] * (frame->height - 1), -frame->linesize[0]);
            } else {
                ret = SDL_UpdateTexture(*tex, NULL, frame->data[0], frame->linesize[0]);
            }
            break;
    }
    return ret;
}

void SDLDst::showFrame(DecodedFramePtr decoded_frame) {
    this->executor_->UI()->add([this, decoded_frame]{
        this->showFrameImpl(decoded_frame);
    });
}


void SDLDst::showFrameImpl(DecodedFramePtr decoded_frame) {
    XLOG(DBG, "showFrameImpl");
    if (!ever_update_wnd_size_from_frame_) {
        static int default_width  = 640;
        static int default_height = 480;
        
        this->default_wnd_width_ = default_width; // decoded_frame->frame->raw->width;
        this->default_wnd_height_ = default_height; // decoded_frame->frame->raw->height;
        this->ever_update_wnd_size_from_frame_ = true;
    }
    
    if (!this->ever_show_wnd_) {
        this->showWnd();
        this->ever_show_wnd_ = true;
    }

    SDL_SetRenderDrawColor(this->renderer_, 0, 0, 0, 255);
    SDL_RenderClear(this->renderer_);
    
    SDL_Rect rect;
    constexpr auto srceen_xleft = 0;
    constexpr auto srceen_ytop = 0;
    calculate_display_rect(&rect, srceen_xleft, srceen_ytop, this->default_wnd_width_, this->default_wnd_height_, decoded_frame->frame->raw->width, decoded_frame->frame->raw->height, decoded_frame->frame->raw->sample_aspect_ratio);
    set_sdl_yuv_conversion_mode(decoded_frame->frame->raw);
    
//    if (!vp->uploaded) {
        if (this->upload_texture(&this->vid_texture_, decoded_frame->frame->raw) < 0) {
            set_sdl_yuv_conversion_mode(NULL);
            return;
        }
//        vp->uploaded = 1;
    const auto flip_v = decoded_frame->frame->raw->linesize[0] < 0;
//    }
    
    SDL_RenderCopyEx(this->renderer_, this->vid_texture_, NULL, &rect, 0, NULL, flip_v ? SDL_FLIP_VERTICAL : SDL_FLIP_NONE);
    set_sdl_yuv_conversion_mode(NULL);
    
    
    
    
    
    SDL_RenderPresent(this->renderer_);
}

void SDLDst::showWnd() {
    int w,h;


    
    w = this->default_wnd_width_; // screen_width ? screen_width : default_width;
    h = this->default_wnd_height_;  // screen_height ? screen_height : default_height;

//    if (!window_title)
//        window_title = input_filename;
    SDL_SetWindowTitle(this->window_, this->env_->file.c_str());

    SDL_SetWindowSize(this->window_, w, h);
    
    constexpr int screen_left = SDL_WINDOWPOS_CENTERED;
    constexpr int screen_top = SDL_WINDOWPOS_CENTERED;
    SDL_SetWindowPosition(this->window_, screen_left, screen_top);
    
    constexpr int is_full_screen = false;
    if (is_full_screen)
        SDL_SetWindowFullscreen(this->window_, SDL_WINDOW_FULLSCREEN_DESKTOP);
    
    SDL_ShowWindow(this->window_);
    SDL_RaiseWindow(this->window_);
}


void SDLDst::loopOnce() {
    SDL_Event event;
    
    double remaining_time = 0.0;
    SDL_PumpEvents();
    while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT) > 0) {
//        if (!cursor_hidden && av_gettime_relative() - cursor_last_shown > CURSOR_HIDE_DELAY) {
//            SDL_ShowCursor(0);
//            cursor_hidden = 1;
//        }
//        if (remaining_time > 0.0)
//            av_usleep((int64_t)(remaining_time * 1000000.0));
//        remaining_time = REFRESH_RATE;
//        if (is->show_mode != SHOW_MODE_NONE && (!is->paused || is->force_refresh))
//            video_refresh(is, &remaining_time);
        SDL_PumpEvents();
    }
    
//    double x;
//    switch (event.type) {
//    case SDL_KEYDOWN:
//        if (exit_on_keydown || event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_q) {
//            do_exit(cur_stream);
//            break;
//        }
//        // If we don't yet have a window, skip all key events, because read_thread might still be initializing...
//        if (!cur_stream->width)
//            continue;
//        switch (event.key.keysym.sym) {
//        case SDLK_f:
//            toggle_full_screen(cur_stream);
//            cur_stream->force_refresh = 1;
//            break;
//        case SDLK_p:
//        case SDLK_SPACE:
//            toggle_pause(cur_stream);
//            break;
//        case SDLK_m:
//            toggle_mute(cur_stream);
//            break;
//        case SDLK_KP_MULTIPLY:
//        case SDLK_0:
//            update_volume(cur_stream, 1, SDL_VOLUME_STEP);
//            break;
//        case SDLK_KP_DIVIDE:
//        case SDLK_9:
//            update_volume(cur_stream, -1, SDL_VOLUME_STEP);
//            break;
//        case SDLK_s: // S: Step to next frame
//            step_to_next_frame(cur_stream);
//            break;
//        case SDLK_a:
//            stream_cycle_channel(cur_stream, AVMEDIA_TYPE_AUDIO);
//            break;
//        case SDLK_v:
//            stream_cycle_channel(cur_stream, AVMEDIA_TYPE_VIDEO);
//            break;
//        case SDLK_c:
//            stream_cycle_channel(cur_stream, AVMEDIA_TYPE_VIDEO);
//            stream_cycle_channel(cur_stream, AVMEDIA_TYPE_AUDIO);
//            stream_cycle_channel(cur_stream, AVMEDIA_TYPE_SUBTITLE);
//            break;
//        case SDLK_t:
//            stream_cycle_channel(cur_stream, AVMEDIA_TYPE_SUBTITLE);
//            break;
//        case SDLK_w:
//            if (cur_stream->show_mode == SHOW_MODE_VIDEO && cur_stream->vfilter_idx < nb_vfilters - 1) {
//                if (++cur_stream->vfilter_idx >= nb_vfilters)
//                    cur_stream->vfilter_idx = 0;
//            } else {
//                cur_stream->vfilter_idx = 0;
//                toggle_audio_display(cur_stream);
//            }
//            break;
//        case SDLK_PAGEUP:
//            if (cur_stream->ic->nb_chapters <= 1) {
//                incr = 600.0;
//                goto do_seek;
//            }
//            seek_chapter(cur_stream, 1);
//            break;
//        case SDLK_PAGEDOWN:
//            if (cur_stream->ic->nb_chapters <= 1) {
//                incr = -600.0;
//                goto do_seek;
//            }
//            seek_chapter(cur_stream, -1);
//            break;
//        case SDLK_LEFT:
//            incr = seek_interval ? -seek_interval : -10.0;
//            goto do_seek;
//        case SDLK_RIGHT:
//            incr = seek_interval ? seek_interval : 10.0;
//            goto do_seek;
//        case SDLK_UP:
//            incr = 60.0;
//            goto do_seek;
//        case SDLK_DOWN:
//            incr = -60.0;
//        do_seek:
//                if (seek_by_bytes) {
//                    pos = -1;
//                    if (pos < 0 && cur_stream->video_stream >= 0)
//                        pos = frame_queue_last_pos(&cur_stream->pictq);
//                    if (pos < 0 && cur_stream->audio_stream >= 0)
//                        pos = frame_queue_last_pos(&cur_stream->sampq);
//                    if (pos < 0)
//                        pos = avio_tell(cur_stream->ic->pb);
//                    if (cur_stream->ic->bit_rate)
//                        incr *= cur_stream->ic->bit_rate / 8.0;
//                    else
//                        incr *= 180000.0;
//                    pos += incr;
//                    stream_seek(cur_stream, pos, incr, 1);
//                } else {
//                    pos = get_master_clock(cur_stream);
//                    if (isnan(pos))
//                        pos = (double)cur_stream->seek_pos / AV_TIME_BASE;
//                    pos += incr;
//                    if (cur_stream->ic->start_time != AV_NOPTS_VALUE && pos < cur_stream->ic->start_time / (double)AV_TIME_BASE)
//                        pos = cur_stream->ic->start_time / (double)AV_TIME_BASE;
//                    stream_seek(cur_stream, (int64_t)(pos * AV_TIME_BASE), (int64_t)(incr * AV_TIME_BASE), 0);
//                }
//            break;
//        default:
//            break;
//        }
//        break;
//    case SDL_MOUSEBUTTONDOWN:
//        if (exit_on_mousedown) {
//            do_exit(cur_stream);
//            break;
//        }
//        if (event.button.button == SDL_BUTTON_LEFT) {
//            static int64_t last_mouse_left_click = 0;
//            if (av_gettime_relative() - last_mouse_left_click <= 500000) {
//                toggle_full_screen(cur_stream);
//                cur_stream->force_refresh = 1;
//                last_mouse_left_click = 0;
//            } else {
//                last_mouse_left_click = av_gettime_relative();
//            }
//        }
//    case SDL_MOUSEMOTION:
//        if (cursor_hidden) {
//            SDL_ShowCursor(1);
//            cursor_hidden = 0;
//        }
//        cursor_last_shown = av_gettime_relative();
//        if (event.type == SDL_MOUSEBUTTONDOWN) {
//            if (event.button.button != SDL_BUTTON_RIGHT)
//                break;
//            x = event.button.x;
//        } else {
//            if (!(event.motion.state & SDL_BUTTON_RMASK))
//                break;
//            x = event.motion.x;
//        }
//            if (seek_by_bytes || cur_stream->ic->duration <= 0) {
//                uint64_t size =  avio_size(cur_stream->ic->pb);
//                stream_seek(cur_stream, size*x/cur_stream->width, 0, 1);
//            } else {
//                int64_t ts;
//                int ns, hh, mm, ss;
//                int tns, thh, tmm, tss;
//                tns  = cur_stream->ic->duration / 1000000LL;
//                thh  = tns / 3600;
//                tmm  = (tns % 3600) / 60;
//                tss  = (tns % 60);
//                frac = x / cur_stream->width;
//                ns   = frac * tns;
//                hh   = ns / 3600;
//                mm   = (ns % 3600) / 60;
//                ss   = (ns % 60);
//                av_log(NULL, AV_LOG_INFO,
//                       "Seek to %2.0f%% (%2d:%02d:%02d) of total duration (%2d:%02d:%02d)       \n", frac*100,
//                        hh, mm, ss, thh, tmm, tss);
//                ts = frac * cur_stream->ic->duration;
//                if (cur_stream->ic->start_time != AV_NOPTS_VALUE)
//                    ts += cur_stream->ic->start_time;
//                stream_seek(cur_stream, ts, 0, 0);
//            }
//        break;
//    case SDL_WINDOWEVENT:
//        switch (event.window.event) {
//            case SDL_WINDOWEVENT_SIZE_CHANGED:
//                screen_width  = cur_stream->width  = event.window.data1;
//                screen_height = cur_stream->height = event.window.data2;
//                if (cur_stream->vis_texture) {
//                    SDL_DestroyTexture(cur_stream->vis_texture);
//                    cur_stream->vis_texture = NULL;
//                }
//                if (vk_renderer)
//                    vk_renderer_resize(vk_renderer, screen_width, screen_height);
//            case SDL_WINDOWEVENT_EXPOSED:
//                cur_stream->force_refresh = 1;
//        }
//        break;
//    case SDL_QUIT:
//    case FF_QUIT_EVENT:
//        do_exit(cur_stream);
//        break;
//    default:
//        break;
//    }
}

}

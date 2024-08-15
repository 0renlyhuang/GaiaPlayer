//
//  engine.cpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/7/5
//  
//
    

#include "engine.hpp"

#include "engine/demux/demuxer.hpp"
#include "engine/engine_di_def.h"
#include "engine/engine_env.hpp"
#include "engine/decode/decoder.hpp"
#include "engine/decode/video/video_decoder.hpp"
#include "engine/decode/audio/audio_decoder.hpp"
#include "engine/destination/sdl_dst.hpp"
#include "engine/decorate/audio_unifier.hpp"
#include "engine/decorate/video_unifier.hpp"
#include "engine/control/clock.hpp"
#include "engine/consume/consume_data_source.hpp"
#include "engine/consume/sdl_audio_consumer.hpp"
#include "engine/consume/sdl_video_consumer.hpp"
#include "common/event/event_center.hpp"

#include "engine/engine_di.h"

#include "base/log/log.hpp"
#include <folly/experimental/coro/AsyncScope.h>
#include <boost/di.hpp>
#include <boost/di/extension/providers/runtime_provider.hpp>
#include <boost/di/extension/injections/extensible_injector.hpp>

namespace gaia::engine {

Engine::Engine(std::shared_ptr<gaia::base::Executors> executor): executors_(executor)  {
    this->dst_ = std::make_shared<SDLDst>();
}

void Engine::play(std::string file) {
    XLOG(INFO) << "Engine::play";
    const auto error = this->dst_->launchWnd();
    if (error.has_value()) {
        return;
    }
    
    
    executors_->Logic()->add([this, file=std::move(file)]{
        this->playImpl(std::move(file));
    });
}

void Engine::seekAfter(std::chrono::milliseconds duration) {
    
}


void Engine::playImpl(std::string file) {
    namespace di = boost::di;
    
    
    using namespace std::chrono_literals;
    base::TimeUnit start_pts = 0s;
    const auto clock = std::make_shared<Clock>(Clock::MasterType::kAudio, start_pts);
    
    this->injector_ = std::make_shared<EngineInjector>(BuildEngineInjector(this->executors_, file, this->dst_, clock));
    
    const auto source = this->injector_->create<std::shared_ptr<FileSource>>();
    const auto env = this->injector_->create<std::shared_ptr<EngineEnv>>();
    env->file = file;
    
    const auto expected_ctx = source->onAttach();
    if (expected_ctx.hasValue()) {
        env->ctx = expected_ctx.value();
    }
    else {
        assert(false);
        XLOG(ERR) << expected_ctx.error();
        return;
    }
    
    using namespace std::chrono_literals;
    env->max_frame_duration = (expected_ctx.value()->iformat->flags & AVFMT_TS_DISCONT) ? 10s : 3600s;
    
    this->sdl_video_consumer_ = this->injector_->create<std::shared_ptr<SDLVideoConsumer>>();
    const auto perf_tracker = this->injector_->create<std::shared_ptr<PerfTracker>>();
    dst_->onAttch(env, executors_, perf_tracker);
    
    
    const auto expected_streams = Demuxer::splitStreams(env->ctx);
    if (expected_streams.hasValue()) {
        env->streams = std::make_shared<Streams>(expected_streams.value());
    } 
    else {
        assert(false);
        XLOG(ERR) << expected_streams.error();
        return;
    }
    
    AVRational fr = av_guess_frame_rate(env->ctx,  env->streams->v_stream, NULL);
    env->guess_frame_rate = fr;
    env->guess_frame_duration = base::toTimeUnit(1, av_inv_q(fr));
    if (env->streams->a_stream->start_time != AV_NOPTS_VALUE) {
        clock->setNow(base::toTimeUnit(env->streams->a_stream->start_time, env->streams->a_stream->time_base));
    }
    else if (env->ctx->iformat->flags & AVFMT_NOTIMESTAMPS && env->ctx->start_time != AV_NOPTS_VALUE) {
        clock->setNow(base::toTimeUnit(env->ctx->start_time, AV_TIME_BASE_Q));
    }
    else {
        clock->setNow(base::TimeUnit(0));
    }
    
   
    const auto demuxer = this->injector_->create<std::shared_ptr<Demuxer>>();
    const auto open_stream_error = demuxer->openStreams(env->ctx, env->streams);
    if (open_stream_error.has_value()) {
        return;
    }
    
    const auto media_probe = this->injector_->create<std::shared_ptr<MediaProbe>>();
    const auto audio_decoder = this->injector_->create<std::shared_ptr<AudioDecoder>>();
    const auto video_decoder = this->injector_->create<std::shared_ptr<VideoDecoder>>();
    media_probe->printMediaInfo(env->ctx, video_decoder->getCodecCtx(), audio_decoder->getCodecCtx());
    
    demuxer->startDemux(env->serial);
    
    
    const auto sdl_consumer = this->injector_->create<std::weak_ptr<SDLAudioConsumer>>();
    dst_->startAudioDevice(sdl_consumer);
    

    
}


void Engine::loopOnce() {
    this->dst_->loopOnce();
}

void Engine::loop() {
    loopOnce();
    
    // TODO: add cancelation
    this->executors_->UI()->add([this]{
        this->loop();
    });
    
}



  

}

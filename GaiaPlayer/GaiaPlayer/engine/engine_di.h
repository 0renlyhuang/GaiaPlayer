//
//  engine_di.h
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/8/8
//  
//
    

#ifndef engine_di_h
#define engine_di_h

#include <boost/di.hpp>

#include "engine/engine_di_def.h"
#include "engine/demux/demuxer.hpp"
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
#include "engine/source/file_source.hpp"
#include "base/async/executors.hpp"
#include "engine/destination/sdl_dst.hpp"
#include "common/perf/perf_tracker.hpp"
#include "common/probe/media_probe.hpp"

#include <type_traits>

namespace gaia::engine {

static inline auto BuildEngineInjector(
                    std::shared_ptr<gaia::base::Executors> executor,
                    const std::string& file,
                    std::shared_ptr<SDLDst> dst,
                    std::shared_ptr<Clock> clock) {
    
    namespace di = boost::di;
    
    return di::make_injector(
                             di::bind<base::Executors>.to(executor),
                             di::bind<Clock>.to(clock),
                             di::bind<std::string>().named(def::input_file_name).to(file),
                             di::bind<FileSource>().to<FileSource>().in(di::singleton),
                             di::bind<EngineEnv>().to<EngineEnv>().in(di::singleton),
                             di::bind<SDLDst>().to(dst),
                             di::bind<Demuxer>().to<Demuxer>().in(di::singleton),
                             di::bind<AudioUnifier>().to<AudioUnifier>().in(di::singleton),
                             di::bind<VideoUnifier>().to<VideoUnifier>().in(di::singleton),
                             di::bind<AudioDecoder>.to<AudioDecoder>().in(di::singleton),
                             di::bind<VideoDecoder>.to<VideoDecoder>().in(di::singleton),
                             di::bind<Decoder>().to<Decoder>().in(di::singleton),
                             di::bind<ConsumeDataSource>().to<ConsumeDataSource>().in(di::singleton),
                             di::bind<SDLVideoConsumer>().to<SDLVideoConsumer>().in(di::singleton),
                             di::bind<SDLAudioConsumer>().to<SDLAudioConsumer>().in(di::singleton),
                             di::bind<EventCenter>().to<EventCenter>().in(di::singleton),
                             di::bind<PerfTracker>().to<PerfTracker>().in(di::singleton),
                             di::bind<MediaProbe>().to<MediaProbe>().in(di::singleton)
                             );
}

using EngineInjector = std::invoke_result_t<
    decltype(BuildEngineInjector),
    std::shared_ptr<gaia::base::Executors>,
    const std::string&,
    std::shared_ptr<SDLDst>,
    std::shared_ptr<Clock>
>;






}

#endif /* engine_di_h */

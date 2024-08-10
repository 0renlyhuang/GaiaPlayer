//
//  sdl_audio_consumer.hpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/8/6
//  
//
    

#ifndef sdl_consumer_hpp
#define sdl_consumer_hpp

#include "engine/engine_env.hpp"
#include "engine/destination/sdl_dst.hpp"
#include "base/async/executors.hpp"
#include "engine/control/clock.hpp"
#include "base/buf/buf.hpp"
#include "engine/consume/consume_data_source.hpp"

#include <folly/CancellationToken.h>

#include <memory>

namespace gaia::engine {

class SDLAudioConsumer: public ISDLDstProvider {
public:
    SDLAudioConsumer(std::shared_ptr<EngineEnv> env, std::shared_ptr<base::Executors> executor, std::shared_ptr<SDLDst> sdl_dst, std::shared_ptr<Clock> clock, std::shared_ptr<ConsumeDataSource> consume_data_source);
    
    base::ErrorMsgOpt consumeAudioBuf(size_t sz, std::byte *dst);
    
    base::ErrorMsgOpt provideAudio(std::byte *dst, size_t sz, ISDLDstProvider::FillAudioImplFunc fill_func_impl) override;
    
    
private:
    folly::Expected<base::BufView, base::ErrorMsg> adapteAudioFormate(FramePtr frame);
    void updateClockWithAudioConsumeSize(size_t sz);
    
    
    std::shared_ptr<EngineEnv> env_;
    std::shared_ptr<base::Executors> executor_;
    std::shared_ptr<SDLDst> sdl_dst_;
    std::shared_ptr<Clock> clock_;
    std::shared_ptr<ConsumeDataSource> data_source_;
    
    std::optional<AudioParams> audio_src_params_;
    SwrContext *swr_ctx_ = nullptr;
    uint8_t *swr_tmp_buf_ = nullptr;
    uint32_t swr_tmp_buf_sz_ = 0;
    
    
    base::Buf audio_buf_rest_;
    size_t audio_buf_idx_ = 0;
    
    folly::CancellationSource cancel_source_;
    size_t consume_sz_of_audio_serial_ = 0;
};

}

#endif /* sdl_consumer_hpp */

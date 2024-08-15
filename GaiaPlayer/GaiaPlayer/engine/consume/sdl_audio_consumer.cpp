//
//  sdl_consumer.cpp
//  GaiaPlayer
//
//  Created by renlyhuang on 2024/8/6
//  
//
    

#include "sdl_audio_consumer.hpp"

#include "base/log/log.hpp"

namespace gaia::engine {


SDLAudioConsumer::SDLAudioConsumer(std::shared_ptr<EngineEnv> env, std::shared_ptr<base::Executors> executor, std::shared_ptr<SDLDst> sdl_dst, std::shared_ptr<Clock> clock, std::shared_ptr<ConsumeDataSource> consume_data_source, std::shared_ptr<AudioDecoder> audio_decoder, std::shared_ptr<Demuxer> demuxer): env_(env), executor_(executor), sdl_dst_(sdl_dst), clock_(clock), data_source_(consume_data_source), audio_decoder_(audio_decoder), demuxer_(demuxer), audio_buf_rest_()  {
    
}

base::ErrorMsgOpt SDLAudioConsumer::provideAudio(std::byte *dst, size_t sz, ISDLDstProvider::FillAudioImplFunc fill_func_impl) {
    size_t consumed_sz = 0;
    while (consumed_sz < sz) {
        const auto require_sz = sz - consumed_sz;
        
        if (this->audio_buf_rest_.sz > this->audio_buf_idx_) {
            const auto rest_buf_sz =  this->audio_buf_rest_.sz - this->audio_buf_idx_;
            
            const auto sz_from_rest = std::min(rest_buf_sz, require_sz);
            
            const auto audio_buf_reset_ptr = this->audio_buf_rest_.raw + this->audio_buf_idx_;
            fill_func_impl(dst + consumed_sz, audio_buf_reset_ptr, sz_from_rest);
            consumed_sz += sz_from_rest;
            
            this->audio_buf_idx_ += sz_from_rest;
            
            if (consumed_sz >= sz) {
                break;
            }
        }
        
        
        const auto decoded_frame_opt = this->audio_decoder_->consumeFrame();
        this->executor_->Logic()->add([this]{
            this->demuxer_->resumeDemuxIfNeeded(this->env_->serial);
        });
        
        
        if (!decoded_frame_opt.hasValue()) {
            return base::error("no audio buf after waiting");
        }
        const auto& decoded_frame = decoded_frame_opt.value();
        
        
        if (decoded_frame.serial != this->env_->serial) {
            continue;
        }
        
        const auto adapted_rlt = this->adapteAudioFormate(decoded_frame.frame);
        if (adapted_rlt.hasError()) {
            return adapted_rlt.error();
        }
        
        const auto& adapted_buf_view = adapted_rlt.value();
        
        const auto sz_from_frame = std::min(adapted_buf_view.sz, require_sz);
        fill_func_impl(dst + consumed_sz, adapted_buf_view.raw, sz_from_frame);
        consumed_sz += sz_from_frame;
        
        
        if (sz_from_frame < adapted_buf_view.sz) {  // copy rest from frame to buf
            const auto rest_sz_of_frame = adapted_buf_view.sz - sz_from_frame;
            
            if (this->audio_buf_rest_.capacity() < rest_sz_of_frame) {
                this->audio_buf_rest_.expandTo(rest_sz_of_frame);
            }
            
            memcpy(this->audio_buf_rest_.raw, adapted_buf_view.raw + sz_from_frame, rest_sz_of_frame);
        }
    }
    
    const auto serial = this->env_->serial;
    this->executor_->Logic()->add([cancelToken = this->cancel_source_.getToken(), this, serial, sz]{
        if (cancelToken.isCancellationRequested()) {
            return;
        }
        
        if (this->env_->serial != serial) {
            return;
        }
        
        this->updateClockWithAudioConsumeSize(sz);
    });
    
    
    return base::noError;
}

folly::Expected<base::BufView, base::ErrorMsg> SDLAudioConsumer::adapteAudioFormate(FramePtr frame) {
    int data_size = av_samples_get_buffer_size(NULL, frame->raw->ch_layout.nb_channels,
                                                           frame->raw->nb_samples,
                                                           AVSampleFormat(frame->raw->format), 1);
    
    int wanted_nb_samples = frame->raw->nb_samples; // synchronize_audio(is, frame->raw->nb_samples);
    
    const auto& audio_hw_params_ref = this->sdl_dst_->getAudioHwParamsRef();
    if (!this->audio_src_params_.has_value()) {
        this->audio_src_params_ = audio_hw_params_ref;
    }
    
    if (frame->raw->format        != this->audio_src_params_->fmt            ||
        av_channel_layout_compare(&frame->raw->ch_layout, &this->audio_src_params_->ch_layout) ||
        frame->raw->sample_rate   != this->audio_src_params_->freq           ||
        (wanted_nb_samples       != frame->raw->nb_samples && !this->swr_ctx_)) {
        int ret;
        swr_free(&this->swr_ctx_);
        ret = swr_alloc_set_opts2(&this->swr_ctx_,
                                  &audio_hw_params_ref.ch_layout, audio_hw_params_ref.fmt, audio_hw_params_ref.freq,
                            &frame->raw->ch_layout, AVSampleFormat(frame->raw->format), frame->raw->sample_rate,
                            0, NULL);
        if (ret < 0 || swr_init(this->swr_ctx_) < 0) {
            swr_free(&this->swr_ctx_);
            return base::unexpected("Cannot create sample rate converter for conversion of %d Hz %s %d channels to %d Hz %s %d channels!\n", frame->raw->sample_rate, av_get_sample_fmt_name(AVSampleFormat(frame->raw->format)), frame->raw->ch_layout.nb_channels,
                                                     this->audio_src_params_->freq, av_get_sample_fmt_name(this->audio_src_params_->fmt), this->audio_src_params_->ch_layout.nb_channels);
        }
        if (av_channel_layout_copy(&this->audio_src_params_->ch_layout, &frame->raw->ch_layout) < 0)
            return base::unexpected("av_channel_layout_copy() failed\n");
        
        this->audio_src_params_->freq = frame->raw->sample_rate;
        this->audio_src_params_->fmt = static_cast<AVSampleFormat>(frame->raw->format);
    }
    
    
    if (!this->swr_ctx_) {
        const auto buf = reinterpret_cast<std::byte*>(frame->raw->data[0]);
        return base::BufView(buf, data_size);
    }
    
    
    const uint8_t **in = (const uint8_t **)frame->raw->extended_data;
    uint8_t **out = &this->swr_tmp_buf_;
    int out_count = (int64_t)wanted_nb_samples * audio_hw_params_ref.freq / frame->raw->sample_rate + 256;
    int out_size  = av_samples_get_buffer_size(NULL, audio_hw_params_ref.ch_layout.nb_channels, out_count, audio_hw_params_ref.fmt, 0);
    int len2;
    if (out_size < 0) {
        return base::unexpected("av_samples_get_buffer_size() failed\n");
    }
    
    if (wanted_nb_samples != frame->raw->nb_samples) {
        if (swr_set_compensation(this->swr_ctx_, (wanted_nb_samples - frame->raw->nb_samples) * audio_hw_params_ref.freq / frame->raw->sample_rate,
                                    wanted_nb_samples * audio_hw_params_ref.freq / frame->raw->sample_rate) < 0) {
            return base::unexpected("swr_set_compensation() failed\n");
        }
    }
    av_fast_malloc(&this->swr_tmp_buf_, &this->swr_tmp_buf_sz_, out_size);
    if (!this->swr_tmp_buf_)
        return base::unexpected("av_fast_malloc() failed\n");
    
    len2 = swr_convert(this->swr_ctx_, out, out_count, in, frame->raw->nb_samples);
    if (len2 < 0) {
        return base::unexpected("swr_convert() failed\n");
    }
    
    if (len2 == out_count) {
        XLOG(WARN) << "audio buffer is probably too small\n";
        if (swr_init(this->swr_ctx_) < 0)
            swr_free(&this->swr_ctx_);
    }
    
    const auto buf = reinterpret_cast<std::byte*>(this->swr_tmp_buf_);
    int resampled_data_size = len2 * audio_hw_params_ref.ch_layout.nb_channels * av_get_bytes_per_sample(audio_hw_params_ref.fmt);
    return base::BufView(buf, resampled_data_size);
}


void SDLAudioConsumer::updateClockWithAudioConsumeSize(size_t sz) {
    const auto& audio_hw_params_ref = this->sdl_dst_->getAudioHwParamsRef();
    
    if (this->consume_sz_of_audio_serial_ >= audio_hw_params_ref.audio_buf_size * 2) {
        auto pass_time = base::toTimeUnit(1, AVRational{ .num=static_cast<int>(sz), .den=audio_hw_params_ref.bytes_per_sec });
        this->clock_->onAudioClockPass(pass_time);
    }
    
    this->consume_sz_of_audio_serial_ += sz;
}


}

#pragma once

#include <xaudio2.h>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// PCM storage remains owned until XAudio2 has consumed it. DestroyVoice waits
// for the audio thread before storage is released, including during a reset.
namespace dusk_xbox_voice {
class Playback {
    struct Lane {
        IXAudio2SourceVoice* voice = nullptr;
        std::deque<std::vector<int16_t>> buffers;
        ~Lane() { if (voice) voice->DestroyVoice(); }
        void retire() {
            XAUDIO2_VOICE_STATE state{};
            voice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
            while (buffers.size() > state.BuffersQueued) buffers.pop_front();
        }
    };
    IXAudio2* engine_ = nullptr;
    IXAudio2MasteringVoice* master_ = nullptr;
    std::map<std::string, std::unique_ptr<Lane>> lanes_;
    std::mutex mutex_;
    float gain_ = 1.0f;

    std::unique_ptr<Lane> make_lane() {
        if (!engine_) return nullptr;
        WAVEFORMATEX format{};
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.nChannels = 2;
        format.nSamplesPerSec = 48000;
        format.wBitsPerSample = 16;
        format.nBlockAlign = 4;
        format.nAvgBytesPerSec = 48000 * 4;
        auto lane = std::make_unique<Lane>();
        if (FAILED(engine_->CreateSourceVoice(&lane->voice, &format)) || !lane->voice)
            return nullptr;
        if (FAILED(lane->voice->SetVolume(gain_)) || FAILED(lane->voice->Start()))
            return nullptr;
        return lane;
    }
public:
    Playback() = default;
    Playback(const Playback&) = delete;
    Playback& operator=(const Playback&) = delete;
    ~Playback() {
        lanes_.clear();
        if (master_) master_->DestroyVoice();
        if (engine_) engine_->Release();
    }
    bool open() {
        std::lock_guard lock(mutex_);
        if (engine_) return master_ != nullptr;
        if (FAILED(XAudio2Create(&engine_, 0, XAUDIO2_DEFAULT_PROCESSOR)) || !engine_) {
            if (engine_) engine_->Release();
            engine_ = nullptr;
            return false;
        }
        if (FAILED(engine_->CreateMasteringVoice(&master_)) || !master_) {
            engine_->Release(); engine_ = nullptr;
            return false;
        }
        return true;
    }
    bool add(const std::string& who) {
        std::lock_guard lock(mutex_);
        if (lanes_.contains(who)) return true;
        if (lanes_.size() >= 16) return false;
        auto lane = make_lane();
        if (!lane) return false;
        lanes_.emplace(who, std::move(lane));
        return true;
    }
    void remove(const std::string& who) {
        std::lock_guard lock(mutex_);
        lanes_.erase(who);
    }
    bool put(const std::string& who, const int16_t* stereo, int frames) {
        if (!stereo || frames <= 0 || frames > 4096) return false;
        std::lock_guard lock(mutex_);
        auto it = lanes_.find(who);
        if (it == lanes_.end()) return false;
        auto& lane = *it->second;
        lane.retire();
        if (lane.buffers.size() >= 16) return false;
        lane.buffers.emplace_back(stereo, stereo + static_cast<size_t>(frames) * 2);
        const auto& owned = lane.buffers.back();
        XAUDIO2_BUFFER buffer{};
        buffer.AudioBytes = static_cast<UINT32>(owned.size() * sizeof(int16_t));
        buffer.pAudioData = reinterpret_cast<const BYTE*>(owned.data());
        if (FAILED(lane.voice->SubmitSourceBuffer(&buffer))) {
            lane.buffers.pop_back();
            return false;
        }
        return true;
    }
    int queued(const std::string& who) {
        std::lock_guard lock(mutex_);
        auto it = lanes_.find(who);
        if (it == lanes_.end()) return 0;
        auto& lane = *it->second;
        lane.retire();
        int frames = 0;
        for (const auto& buffer : lane.buffers) frames += static_cast<int>(buffer.size() / 2);
        return frames;
    }
    void clear(const std::string& who) {
        std::lock_guard lock(mutex_);
        auto it = lanes_.find(who);
        if (it == lanes_.end()) return;
        it->second.reset();
        auto lane = make_lane();
        if (lane) it->second = std::move(lane);
        else lanes_.erase(it);
    }
    void set_gain(float gain) {
        std::lock_guard lock(mutex_);
        gain_ = std::isfinite(gain) ? std::clamp(gain, 0.0f, 2.0f) : 1.0f;
        for (auto& [who, lane] : lanes_) lane->voice->SetVolume(gain_);
    }
    void set_peer_gain(const std::string& who, float gain) {
        std::lock_guard lock(mutex_);
        auto it = lanes_.find(who);
        if (it != lanes_.end()) it->second->voice->SetVolume(
            std::isfinite(gain) ? std::clamp(gain, 0.0f, 2.0f) : 1.0f);
    }
    bool pause(const std::string& who) {
        std::lock_guard lock(mutex_);
        auto it = lanes_.find(who);
        return it != lanes_.end() && !FAILED(it->second->voice->Stop());
    }
    bool resume(const std::string& who) {
        std::lock_guard lock(mutex_);
        auto it = lanes_.find(who);
        return it != lanes_.end() && !FAILED(it->second->voice->Start());
    }
};
}

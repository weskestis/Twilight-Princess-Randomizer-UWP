#pragma once
#include <cassert>
#include <cstdint>
#include <deque>
#include <vector>
using HRESULT = int;
using UINT32 = uint32_t;
using BYTE = uint8_t;
using WORD = uint16_t;
#define FAILED(result) ((result) < 0)
constexpr int XAUDIO2_DEFAULT_PROCESSOR = 0, XAUDIO2_VOICE_NOSAMPLESPLAYED = 1;
constexpr int WAVE_FORMAT_PCM = 1;
struct WAVEFORMATEX {
    WORD wFormatTag{}, nChannels{}, wBitsPerSample{}, nBlockAlign{};
    UINT32 nSamplesPerSec{}, nAvgBytesPerSec{};
};
struct XAUDIO2_VOICE_STATE { UINT32 BuffersQueued{}; };
struct XAUDIO2_BUFFER { UINT32 AudioBytes{}; const BYTE* pAudioData{}; };
namespace fake_xaudio {
inline bool failEngine = false, failMaster = false, failSource = false, failStart = false, failSubmit = false;
inline int engines = 0, masters = 0, sources = 0;
inline int reads = 0;
inline float volume = 0;
}
struct IXAudio2SourceVoice {
    std::deque<XAUDIO2_BUFFER> queue;
    IXAudio2SourceVoice() { ++fake_xaudio::sources; }
    void check_storage() {
        for (const auto& buffer : queue) {
            // ASan diagnoses releasing storage before the consuming audio voice.
            for (UINT32 n = 0; n < buffer.AudioBytes; ++n)
                fake_xaudio::reads += buffer.pAudioData[n];
        }
    }
    HRESULT SetVolume(float value) { fake_xaudio::volume = value; return 0; }
    HRESULT Start() { return fake_xaudio::failStart ? -1 : 0; }
    HRESULT Stop() { return 0; }
    void GetState(XAUDIO2_VOICE_STATE* state, UINT32) {
        check_storage(); state->BuffersQueued = static_cast<UINT32>(queue.size());
    }
    HRESULT SubmitSourceBuffer(const XAUDIO2_BUFFER* buffer) {
        if (fake_xaudio::failSubmit) return -1;
        queue.push_back(*buffer); return 0;
    }
    void DestroyVoice() { check_storage(); --fake_xaudio::sources; delete this; }
};
struct IXAudio2MasteringVoice {
    IXAudio2MasteringVoice() { ++fake_xaudio::masters; }
    void DestroyVoice() { assert(fake_xaudio::sources == 0); --fake_xaudio::masters; delete this; }
};
struct IXAudio2 {
    std::vector<IXAudio2SourceVoice*> voices;
    IXAudio2() { ++fake_xaudio::engines; }
    HRESULT CreateMasteringVoice(IXAudio2MasteringVoice** voice) {
        if (fake_xaudio::failMaster) return -1;
        *voice = new IXAudio2MasteringVoice; return 0;
    }
    HRESULT CreateSourceVoice(IXAudio2SourceVoice** voice, const WAVEFORMATEX* format) {
        assert(format->nChannels == 2 && format->nSamplesPerSec == 48000);
        assert(format->nBlockAlign == 4 && format->wBitsPerSample == 16);
        if (fake_xaudio::failSource) return -1;
        *voice = new IXAudio2SourceVoice; voices.push_back(*voice); return 0;
    }
    void Release() { assert(fake_xaudio::masters == 0); --fake_xaudio::engines; delete this; }
};
inline IXAudio2* fake_current_engine = nullptr;
inline HRESULT XAudio2Create(IXAudio2** engine, int, int) {
    *engine = new IXAudio2; fake_current_engine = *engine;
    return fake_xaudio::failEngine ? -1 : 0;
}

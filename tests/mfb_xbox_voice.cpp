#include "dusklight_online/game/voice_chat.hpp"
#include <opus.h>
#include <xaudio2.h>
#include <array>
#include <cassert>
#include <cmath>
#include <span>

int main() {
    using dusklight_online::game::VoiceChat;
    int error = OPUS_OK;
    OpusEncoder* encoder = opus_encoder_create(48000, 1, OPUS_APPLICATION_VOIP, &error);
    assert(error == OPUS_OK && encoder);
    std::array<int16_t, 960> pcm{};
    for (size_t n = 0; n < pcm.size(); ++n)
        pcm[n] = static_cast<int16_t>(std::sin(static_cast<double>(n) * 0.15) * 6000);
    std::array<unsigned char, 400> packet{};
    const int bytes = opus_encode(encoder, pcm.data(), 960, packet.data(), packet.size());
    assert(bytes > 0 && bytes <= 400);
    const std::span<const uint8_t> encoded(packet.data(), static_cast<size_t>(bytes));
    {
        VoiceChat voice;
        assert(voice.input_devices().empty());
        voice.receive("guest", 1, encoded, 1.0f, 0.5f);
        assert(fake_xaudio::sources == 0);
        voice.configure(true, true, "", 100, 100, false, 1.0f);
        assert(voice.capture().empty());
        voice.receive("guest", 0, encoded, 1.0f, 0.5f);
        voice.receive("", 1, encoded, 1.0f, 0.5f);
        voice.receive("guest", 1, {}, 1.0f, 0.5f);
        assert(fake_xaudio::sources == 0);
        for (uint32_t sequence = 1; sequence <= 6; ++sequence)
            voice.receive("guest", sequence, encoded, 1.0f, 0.5f);
        assert(fake_xaudio::sources == 1);
        auto* lane = fake_current_engine->voices.back();
        const size_t queued = lane->queue.size();
        assert(queued >= 4 && queued <= 16);
        voice.receive("guest", 6, encoded, 1.0f, 0.5f);
        voice.receive("guest", 5, encoded, 1.0f, 0.5f);
        assert(lane->queue.size() == queued);
        voice.receive("guest", 10, encoded, 1.0f, 0.5f);
        assert(lane->queue.size() > queued && lane->queue.size() <= 16);
        for (uint32_t sequence = 11; sequence < 60; ++sequence)
            voice.receive("guest", sequence, encoded, 1.0f, 0.5f);
        assert(fake_xaudio::sources == 1 && fake_current_engine->voices.back()->queue.size() <= 16);
        voice.peer_left("guest");
        assert(fake_xaudio::sources == 0);
        voice.receive("guest", 1, encoded, 0.5f, 0.0f);
        assert(fake_xaudio::sources == 1);
        voice.stop();
        assert(fake_xaudio::sources == 0 && voice.capture().empty());
        voice.receive("guest", 2, encoded, 1.0f, 0.5f);
        assert(fake_xaudio::sources == 0);
    }
    opus_encoder_destroy(encoder);
    assert(fake_xaudio::engines == 0 && fake_xaudio::masters == 0 && fake_xaudio::sources == 0);
}

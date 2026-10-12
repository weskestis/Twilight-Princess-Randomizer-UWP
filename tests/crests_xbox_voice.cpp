#include "voice_io.hpp"
#include <opus.h>
#include <xaudio2.h>
#include <array>
#include <cassert>
#include <cmath>

int main() {
    int error = OPUS_OK;
    OpusEncoder* encoder = opus_encoder_create(48000, 1, OPUS_APPLICATION_VOIP, &error);
    assert(error == OPUS_OK && encoder);
    std::array<int16_t, 960> pcm{};
    for (size_t n = 0; n < pcm.size(); ++n)
        pcm[n] = static_cast<int16_t>(std::sin(static_cast<double>(n) * 0.15) * 6000);
    std::array<unsigned char, 400> packet{};
    const int bytes = opus_encode(encoder, pcm.data(), 960, packet.data(), packet.size());
    assert(bytes > 0 && bytes <= 400);
    {
        voice_io::Device voice;
        voice.play("guest", 1, packet.data(), bytes, 1.0f, 0.0f);
        assert(fake_xaudio::sources == 0);
        voice.set(true, false, "", 1.0f, 1.0f);
        assert(voice.take().empty());
        for (uint32_t sequence = 1; sequence <= 6; ++sequence)
            voice.play("guest", sequence, packet.data(), bytes, 1.0f, 0.0f);
        assert(fake_xaudio::sources == 1);
        auto* lane = fake_current_engine->voices.back();
        const size_t queued = lane->queue.size();
        assert(queued > 0 && queued <= 16);
        voice.play("guest", 6, packet.data(), bytes, 1.0f, 0.0f);
        voice.play("guest", 5, packet.data(), bytes, 1.0f, 0.0f);
        assert(lane->queue.size() == queued);
        voice.play("guest", 8, packet.data(), bytes, 1.0f, 0.0f);
        assert(lane->queue.size() > queued);
        for (uint32_t sequence = 9; sequence < 60; ++sequence)
            voice.play("guest", sequence, packet.data(), bytes, 1.0f, 0.0f);
        assert(fake_xaudio::sources == 1 && fake_current_engine->voices.back()->queue.size() <= 16);
        voice.forget("guest");
        assert(fake_xaudio::sources == 0);
        voice.play("guest", 1, packet.data(), bytes, 1.0f, -1.0f);
        assert(fake_xaudio::sources == 1);
        voice.close();
        assert(fake_xaudio::sources == 0 && voice.take().empty());
    }
    opus_encoder_destroy(encoder);
    assert(fake_xaudio::engines == 0 && fake_xaudio::masters == 0 && fake_xaudio::sources == 0);
}

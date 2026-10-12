#include "xbox_voice_playback.hpp"
#include <cassert>
#include <limits>

int main() {
    using dusk_xbox_voice::Playback;
    using namespace fake_xaudio;
    {
        failEngine = true; Playback failed;
        assert(!failed.open() && engines == 0); failEngine = false;
    }
    {
        failMaster = true; Playback failed;
        assert(!failed.open() && engines == 0); failMaster = false;
    }
    {
        Playback playback;
        assert(playback.open());
        failSource = true; assert(!playback.add("failed")); failSource = false;
        failStart = true; assert(!playback.add("failed")); failStart = false;
        assert(sources == 0);
        assert(playback.add("first"));
        assert(playback.pause("first") && playback.resume("first"));
        assert(!playback.pause("absent") && !playback.resume("absent"));
        int16_t pcm[1920]; std::fill_n(pcm, 1920, int16_t{7});
        assert(!playback.put("unknown", pcm, 960));
        assert(!playback.put("first", nullptr, 960));
        assert(!playback.put("first", pcm, 4097));
        for (int n = 0; n < 16; ++n) assert(playback.put("first", pcm, 960));
        assert(!playback.put("first", pcm, 960));
        assert(playback.queued("first") == 16 * 960);
        // The caller's frame can be reused as soon as put returns.
        std::fill_n(pcm, 1920, int16_t{99});
        auto* voice = fake_current_engine->voices.back();
        assert(reinterpret_cast<const int16_t*>(voice->queue.front().pAudioData)[0] == 7);
        for (int n = 0; n < 8; ++n) voice->queue.pop_front();
        assert(playback.queued("first") == 8 * 960);
        assert(playback.put("first", pcm, 960));
        failSubmit = true; assert(!playback.put("first", pcm, 960)); failSubmit = false;
        assert(playback.queued("first") == 9 * 960);
        playback.set_gain(std::numeric_limits<float>::quiet_NaN()); assert(volume == 1.0f);
        playback.set_gain(9.0f); assert(volume == 2.0f);
        playback.clear("first"); assert(playback.queued("first") == 0 && sources == 1);
        for (int n = 1; n < 16; ++n) assert(playback.add(std::to_string(n)));
        assert(!playback.add("overflow"));
        assert(playback.put("first", pcm, 960));
        playback.remove("first"); assert(sources == 15);
        assert(playback.add("replacement"));
        assert(playback.put("replacement", pcm, 960));
        failSource = true; playback.clear("replacement"); failSource = false;
        assert(playback.queued("replacement") == 0 && sources == 15);
        assert(playback.add("replacement"));
        assert(playback.put("replacement", pcm, 960));
    }
    assert(reads > 0 && engines == 0 && masters == 0 && sources == 0);
}

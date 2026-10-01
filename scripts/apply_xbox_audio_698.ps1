$ErrorActionPreference = 'Stop'

function Read-Lf([string]$Path) {
    return [IO.File]::ReadAllText($Path).Replace(([string][char]13 + [char]10), [string][char]10)
}

function Replace-Once([string]$Text, [string]$Old, [string]$New, [string]$Label) {
    $first = $Text.IndexOf($Old, [StringComparison]::Ordinal)
    if ($first -lt 0) {
        throw "Missing .698 transform anchor: $Label"
    }
    $second = $Text.IndexOf($Old, $first + $Old.Length, [StringComparison]::Ordinal)
    if ($second -ge 0) {
        throw "Ambiguous .698 transform anchor: $Label"
    }
    return $Text.Substring(0, $first) + $New + $Text.Substring($first + $Old.Length)
}

$audioPath = "$env:TPR_SRC\src\dusk\audio\DuskAudioSystem.cpp"
$cmakePath = "$env:TPR_SRC\platforms\uwp\CMakeLists.txt"
$audio = Read-Lf $audioPath
$cmake = Read-Lf $cmakePath

if (-not $audio.Contains('#include <array>')) {
    throw 'Missing .698 transform anchor: <array> include'
}
if (-not $audio.Contains('#include <algorithm>')) {
    $audio = $audio.Replace('#include <array>', @'
#include <algorithm>
#include <array>
#include <deque>
#include <memory>
#include <vector>
#if defined(_UWP)
#include <xaudio2.h>
#endif
'@)
}

$old = 'static SDL_AudioStream* PlaybackStream;'
$new = @'
static SDL_AudioStream* PlaybackStream;
#if defined(_UWP)
static IXAudio2* XboxAudioEngine = nullptr;
static IXAudio2MasteringVoice* XboxMasterVoice = nullptr;
static IXAudio2SourceVoice* XboxSourceVoice = nullptr;
static std::deque<std::shared_ptr<std::vector<f32>>> XboxAudioBuffers;
static bool XboxAudioPaused = false;
#endif
'@
$audio = Replace-Once $audio $old $new 'audio backend globals'

$old = 'static bool InitSDL3Output() {'
$new = @'
#if defined(_UWP)
static void ShutdownXboxAudio() {
    XboxAudioBuffers.clear();
    if (XboxSourceVoice) {
        XboxSourceVoice->Stop();
        XboxSourceVoice->FlushSourceBuffers();
        XboxSourceVoice->DestroyVoice();
        XboxSourceVoice = nullptr;
    }
    if (XboxMasterVoice) {
        XboxMasterVoice->DestroyVoice();
        XboxMasterVoice = nullptr;
    }
    if (XboxAudioEngine) {
        XboxAudioEngine->Release();
        XboxAudioEngine = nullptr;
    }
}

static bool InitXboxAudio() {
    ShutdownXboxAudio();
    dusk::startup_guard::stage("audio.xaudio-create");
    HRESULT hr = XAudio2Create(&XboxAudioEngine, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr) || !XboxAudioEngine) {
        dusk::startup_guard::stage("audio.xaudio-create-failed");
        return false;
    }

    dusk::startup_guard::stage("audio.xaudio-master");
    hr = XboxAudioEngine->CreateMasteringVoice(&XboxMasterVoice);
    if (FAILED(hr) || !XboxMasterVoice) {
        dusk::startup_guard::stage("audio.xaudio-master-failed");
        ShutdownXboxAudio();
        return false;
    }

    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    format.nChannels = 2;
    format.nSamplesPerSec = SampleRate;
    format.wBitsPerSample = 32;
    format.nBlockAlign = static_cast<WORD>(format.nChannels * sizeof(float));
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

    dusk::startup_guard::stage("audio.xaudio-source");
    hr = XboxAudioEngine->CreateSourceVoice(&XboxSourceVoice, &format);
    if (FAILED(hr) || !XboxSourceVoice) {
        dusk::startup_guard::stage("audio.xaudio-source-failed");
        ShutdownXboxAudio();
        return false;
    }

    OutChannelCount = 2;
    EnableHrtf = dusk::getSettings().audio.outputMode.getValue() ==
                 dusk::AudioOutputMode::StereoHeadphones;

    hr = XboxSourceVoice->Start();
    if (FAILED(hr)) {
        dusk::startup_guard::stage("audio.xaudio-start-failed");
        ShutdownXboxAudio();
        return false;
    }

    XboxAudioPaused = false;
    dusk::startup_guard::stage("audio.xaudio-ready");
    return true;
}
#endif

static bool InitSDL3Output() {
'@
$audio = Replace-Once $audio $old $new 'XAudio2 helper insertion'

$old = '    const bool outputReady = InitSDL3Output();'
$new = @'
#if defined(_UWP)
    const bool outputReady = InitXboxAudio();
#else
    const bool outputReady = InitSDL3Output();
#endif
'@
$audio = Replace-Once $audio $old $new 'audio output backend selection'

$nl = [string][char]10
$start = $audio.IndexOf('void dusk::audio::Reinitialize() {', [StringComparison]::Ordinal)
$end = $audio.IndexOf($nl + '}' + $nl + $nl + 'void dusk::audio::Shutdown', $start, [StringComparison]::Ordinal)
if ($start -lt 0 -or $end -lt 0) {
    throw 'Missing .698 transform anchor: Reinitialize function'
}
$old = $audio.Substring($start, $end - $start + 2)
$new = @'
void dusk::audio::Reinitialize() {
#if defined(_UWP)
    InitXboxAudio();
#else
    // don't re-init unless we've initialized first (using PlaybackStream being set as proxy)
    if (PlaybackStream && InitSDL3Output()) {
        SDL_ResumeAudioStreamDevice(PlaybackStream);
    }
#endif
}
'@
$audio = $audio.Replace($old, $new)

$start = $audio.IndexOf('void dusk::audio::Shutdown() {', [StringComparison]::Ordinal)
$end = $audio.IndexOf($nl + '}' + $nl + $nl + 'void dusk::audio::Pump', $start, [StringComparison]::Ordinal)
if ($start -lt 0 -or $end -lt 0) {
    throw 'Missing .698 transform anchor: Shutdown function'
}
$old = $audio.Substring($start, $end - $start + 2)
$new = @'
void dusk::audio::Shutdown() {
#if defined(_UWP)
    ShutdownXboxAudio();
#else
    if (PlaybackStream) {
        SDL_DestroyAudioStream(PlaybackStream);
        PlaybackStream = nullptr;
    }
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
#endif
}
'@
$audio = $audio.Replace($old, $new)

$start = $audio.IndexOf('void dusk::audio::SetPaused(const bool paused) {', [StringComparison]::Ordinal)
$end = $audio.IndexOf($nl + '}' + $nl + $nl + 'void dusk::audio::SetEnableReverb', $start, [StringComparison]::Ordinal)
if ($start -lt 0 -or $end -lt 0) {
    throw 'Missing .698 transform anchor: SetPaused function'
}
$old = $audio.Substring($start, $end - $start + 2)
$new = @'
void dusk::audio::SetPaused(const bool paused) {
#if defined(_UWP)
    if (!XboxSourceVoice || paused == XboxAudioPaused) {
        return;
    }
    if (paused) {
        XboxSourceVoice->Stop();
    } else {
        XboxSourceVoice->Start();
    }
    XboxAudioPaused = paused;
#else
    if (!PlaybackStream) {
        return;
    }
    if (paused) {
        SDL_PauseAudioStreamDevice(PlaybackStream);
    } else {
        SDL_ResumeAudioStreamDevice(PlaybackStream);
    }
#endif
}
'@
$audio = $audio.Replace($old, $new)

$start = $audio.IndexOf('void dusk::audio::Pump() {', [StringComparison]::Ordinal)
$end = $audio.IndexOf($nl + '}' + $nl + $nl + 'void dusk::audio::SetMasterVolume', $start, [StringComparison]::Ordinal)
if ($start -lt 0 -or $end -lt 0) {
    throw 'Missing .698 transform anchor: Pump function'
}
$old = $audio.Substring($start, $end - $start + 2)
$new = @'
void dusk::audio::Pump() {
#if defined(_UWP)
    if (!XboxSourceVoice || XboxAudioPaused || OutChannelCount != 2) {
        return;
    }

    XAUDIO2_VOICE_STATE state{};
    XboxSourceVoice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
    while (XboxAudioBuffers.size() > state.BuffersQueued) {
        XboxAudioBuffers.pop_front();
    }

    constexpr size_t targetBuffers = 8;
    while (XboxAudioBuffers.size() < targetBuffers) {
        const u32 countSubframes = JASDriver::getSubFrames();
        const size_t samplesPerSubframe = static_cast<size_t>(DSP_SUBFRAME_SIZE) * 2;
        auto frame = std::make_shared<std::vector<f32>>(
            static_cast<size_t>(countSubframes) * samplesPerSubframe);

        {
            JASCriticalSection section;
            JASAudioThread::setDSPSyncCount(countSubframes);
            for (u32 i = 0; i < countSubframes; ++i) {
                const int rendered = RenderAudioSubframe();
                if (rendered <= 0) {
                    return;
                }
                std::copy_n(OutInterleaveBufferFull.data(), samplesPerSubframe,
                    frame->data() + static_cast<size_t>(i) * samplesPerSubframe);
                JASAudioThread::snIntCount -= 1;
            }
        }

        XAUDIO2_BUFFER buffer{};
        buffer.AudioBytes = static_cast<UINT32>(frame->size() * sizeof(f32));
        buffer.pAudioData = reinterpret_cast<const BYTE*>(frame->data());
        const HRESULT hr = XboxSourceVoice->SubmitSourceBuffer(&buffer);
        if (FAILED(hr)) {
            dusk::startup_guard::stage("audio.xaudio-submit-failed");
            return;
        }
        XboxAudioBuffers.push_back(std::move(frame));
    }
#else
    if (!PlaybackStream || OutChannelCount == 0) {
        return;
    }

    int queued = SDL_GetAudioStreamQueued(PlaybackStream);
    if (queued < 0) {
        return;
    }

    const int bytesPerSecond = SampleRate * static_cast<int>(OutChannelCount) *
                               static_cast<int>(sizeof(float));
    const int targetQueuedBytes = bytesPerSecond * 80 / 1000;
    for (int frame = 0; frame < 16 && queued < targetQueuedBytes; ++frame) {
        const int rendered = RenderNewAudioFrame();
        if (rendered <= 0) {
            break;
        }
        queued += rendered;
    }
#endif
}
'@
$audio = $audio.Replace($old, $new)

$old = '    SDL_PutAudioStreamData(PlaybackStream, OutInterleaveBuffer.data(), bytesToWrite);'
$new = @'
#if !defined(_UWP)
    SDL_PutAudioStreamData(PlaybackStream, OutInterleaveBuffer.data(), bytesToWrite);
#endif
'@
$audio = Replace-Once $audio $old $new 'audio subframe SDL write'

$old = @'
    Winhttp.lib
    Ws2_32.lib)
'@
$new = @'
    Winhttp.lib
    Ws2_32.lib
    xaudio2.lib)
'@
$cmake = Replace-Once $cmake $old $new 'UWP XAudio2 link library'

foreach ($marker in @(
    'InitXboxAudio()',
    'XAudio2Create(&XboxAudioEngine',
    'CreateMasteringVoice(&XboxMasterVoice)',
    'CreateSourceVoice(&XboxSourceVoice',
    'SubmitSourceBuffer(&buffer)',
    'audio.xaudio-ready',
    'xaudio2.lib'))
{
    if (-not ($audio.Contains($marker) -or $cmake.Contains($marker))) {
        throw "Missing .698 XAudio2 marker: $marker"
    }
}

[IO.File]::WriteAllText($audioPath, $audio, [Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllText($cmakePath, $cmake, [Text.UTF8Encoding]::new($false))

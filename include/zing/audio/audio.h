#pragma once

#include <zing/pch.h>

#include <zing/audio/audio_analysis_settings.h>
#include <zing/audio/audio_device_settings.h>
#include <zing/audio/audio_samples.h>

extern "C" {
#include <soundpipe/h/soundpipe.h>
}

struct tsf;
#ifdef WIN32
#define LINK_PLATFORM_WINDOWS
#elif defined __APPLE__
#define LINK_PLATFORM_MACOSX
#else
#define LINK_PLATFORM_LINUX
#endif
#include <ableton/link/HostTimeFilter.hpp>
#include <ableton/platforms/Config.hpp>
#include <ableton/Link.hpp>

#include <kiss_fft.h>

union SDL_Event;

namespace Zing
{

struct AudioBundle
{
    uint32_t channel;
    std::vector<float> data;
};

struct AudioSettings
{
    std::atomic<bool> enableMetronome = false;
};

using AudioCB = std::function<void(const std::chrono::microseconds hostTime, void* pOutput, uint32_t frameCount)>;

struct ApiInfo
{
    int NumOutDevices() const
    {
        return int(outDeviceNames.size());
    }
    int NumInDevices() const
    {
        return int(inDeviceNames.size());
    }
    std::vector<uint32_t> outDeviceApiIndices;
    std::vector<std::string> outDeviceNames;
    std::map<uint32_t, std::vector<uint32_t>> outSampleRates;

    std::vector<uint32_t> inDeviceApiIndices;
    std::vector<std::string> inDeviceNames;
    std::map<uint32_t, std::vector<uint32_t>> inSampleRates;
};

struct AudioChannelState
{
    uint32_t frames = 0;
    int64_t totalFrames = 0;
    uint32_t channelCount = 2;
    uint32_t sampleRate = 44000;
    double deltaTime = 1.0f / (double)sampleRate;
};

struct AudioAnalysisData
{
    // Double buffer the data
    std::vector<float> spectrumBuckets;
    std::vector<float> spectrum;
    std::vector<float> audio;
    uint32_t currentBuffer = 0;
    std::vector<float> frameCache;
};

struct SpectrumPartitionSettings
{
    uint32_t limit = 0;
    uint32_t n = 0;
    float sharpness = 1.0f;
};

inline bool operator==(const SpectrumPartitionSettings& a, const SpectrumPartitionSettings& b)
{
    return ((a.limit == b.limit) && (a.n == b.n) && (a.sharpness == b.sharpness));
}

struct AudioAnalysis
{
    // FFT
    kiss_fft_cfg cfg;
    std::vector<std::complex<float>> fftIn;
    std::vector<std::complex<float>> fftOut;
    std::vector<float> fftMag;
    std::vector<float> window;

    AudioChannelState channel;
    uint32_t thisChannel;

    uint32_t outputSamples = 0; // The FFT output frames

    float totalWin = 0.0f;

    float currentMaxSpectrum;
    uint32_t maxSpectrumIndex;

    std::atomic<glm::vec4> spectrumBands = glm::vec4(0.0);

    bool fftConfigured = false;
    bool audioActive = false;
    std::atomic_bool quitThread = true;
    std::atomic_bool exited = true;
    std::thread analysisThread;

    std::vector<float> spectrumPartitions;
    SpectrumPartitionSettings lastSpectrumPartitions;
    bool logPartitions = true;

    // Bundles pending processing
    moodycamel::ConcurrentQueue<std::shared_ptr<AudioBundle>> processBundles;


    moodycamel::ConcurrentQueue<std::shared_ptr<AudioAnalysisData>> analysisData;
    moodycamel::ConcurrentQueue<std::shared_ptr<AudioAnalysisData>> analysisDataCache;
};

struct LinkData
{
    double requestedTempo = 60.0;
    bool requestStart = false;
    bool requestStop = false;
    double quantum = 4.0;
    bool startStopSyncOn = true;
};

using fnMidiBroadcast = std::function<void(const tml_message&)>;

struct AudioContext
{
    bool m_initialized = false;
    bool m_isPlaying = true;
    bool m_audioValid = false;
    bool m_changedDeviceCombo = true;

    std::atomic<bool> m_playMetronome = false;
    AudioCB m_fnCallback = nullptr;

    AudioChannelState inputState;
    AudioChannelState outputState;

    AudioSettings settings;
    AudioDeviceSettings audioDeviceSettings;

    // Audio analysis information. Processed outside of audio thread, consumed in UI,
    // so use system mutex, we don't need to spin
    std::vector<std::shared_ptr<AudioAnalysis>> analysisChannels;
    AudioAnalysisSettings audioAnalysisSettings;

    std::atomic<uint64_t> analysisWriteGeneration = 0;
    std::atomic<uint64_t> analysisReadGeneration = 0;

    double m_sampleTime = 0.0;
    std::thread::id threadId;
    std::vector<std::string> m_deviceNames;
    std::vector<std::string> m_apiNames;
    std::map<uint32_t, ApiInfo> m_mapApis;
    std::vector<std::string> m_currentRateNames;
    std::vector<uint32_t> m_currentRates;

    // SoundPipe
    sp_data* pSP = nullptr;

    // PortAudio
    PaStreamParameters m_inputParams;
    PaStreamParameters m_outputParams;
    PaStream* m_pStream = nullptr;
    
    // Bundles of audio data passed out of the audio thread to analysis
    moodycamel::ConcurrentQueue<std::shared_ptr<AudioBundle>> spareBundles;

    // Link
    std::atomic<std::chrono::microseconds> m_outputLatency;
    ableton::link::HostTimeFilter<ableton::link::platform::Clock> m_hostTimeFilter;
    std::mutex m_linkDataGuard;
    LinkData m_linkData;
    LinkData m_lockFreeLinkData;
    ableton::Link m_link = ableton::Link(20.0);
    std::chrono::microseconds m_timeAtLastClick;    // Metronome
    bool m_clicking = false;
    double m_clickFrequency;
    double m_clickTime = 0.0;
    int64_t m_lastClickBeat = 0;
    double m_tempo;
    int m_numPeers;

    AudioSamples m_samples;

    // Midi
    moodycamel::ConcurrentQueue<tml_message> midi;

    std::vector<fnMidiBroadcast> midiClients;
};

AudioContext& GetAudioContext();

inline glm::uvec4 Div(const glm::uvec4& val, uint32_t div)
{
    return glm::uvec4(val.x / div, val.y / div, val.z / div, val.w / div);
}

void audio_add_settings_hooks();
bool audio_init(const AudioCB& fnCallback);
void audio_destroy();
void audio_show_link_gui();
void audio_show_settings_gui();

std::shared_ptr<AudioBundle> audio_get_bundle();
void audio_retire_bundle(std::shared_ptr<AudioBundle>& pBundle);

std::string audio_to_channel_name(uint32_t channel);
void audio_add_midi_event(const tml_message& msg);

#define CHECK_NOT_AUDIO_THREAD assert(std::this_thread::get_id() != ctx.threadId);

// Can't currently use this one since audio threads might be in a pool.  TLS?
#define CHECK_AUDIO_THREAD assert(std::this_thread::get_id() == ctx.threadId);

} // namespace Zing

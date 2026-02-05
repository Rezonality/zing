#pragma once

#include <zing/pch.h>

#include <zest/thread/thread_utils.h>

#include <zing/audio/audio_analysis_settings.h>
#include <zing/audio/audio_device_settings.h>
#include <zing/audio/audio_samples.h>

#include <libremidi/libremidi.hpp>

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
#include <ableton/Link.hpp>
#include <ableton/link/HostTimeFilter.hpp>
#include <ableton/platforms/Config.hpp>

#include <kiss_fftr.h>

union SDL_Event;

namespace Zing
{

// Channel_In/Out/?, count
using ChannelId = std::pair<uint32_t, uint32_t>;
constexpr uint32_t Channel_Out = 0;
constexpr uint32_t Channel_In = 1;

struct AudioBundle
{
    ChannelId channel;
    std::vector<float> data;
};

struct AudioSettings
{
    std::atomic<bool> enableMetronome = false;
    std::atomic<bool> enableMidi = true;
};

using AudioCB = std::function<void(const std::chrono::microseconds hostTime, const void* pInput, void* pOutput, uint32_t frameCount)>;

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
};

inline bool operator==(const SpectrumPartitionSettings& a, const SpectrumPartitionSettings& b)
{
    return ((a.limit == b.limit) && (a.n == b.n));
}

// Channel_In/Out/?, count
using ChannelId = std::pair<uint32_t, uint32_t>;

struct AudioAnalysis
{
    // FFT
    kiss_fftr_cfg cfg;
    std::vector<kiss_fft_scalar> fftIn;
    std::vector<kiss_fft_cpx> fftOut;
    std::vector<float> fftMag;
    std::vector<float> window;

    AudioChannelState channel;
    ChannelId thisChannel;

    std::vector<float> inputCache;
    uint32_t maxInputSize = 48000 * 10;
    fs::path inputDumpPath;

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
    std::vector<float> spectrumBucketsEma;

    // Bundles pending processing
    moodycamel::ConcurrentQueue<std::shared_ptr<AudioBundle>> processBundles;

    moodycamel::ConcurrentQueue<std::shared_ptr<AudioAnalysisData>> analysisData;
    moodycamel::ConcurrentQueue<std::shared_ptr<AudioAnalysisData>> analysisDataCache;

    // Current UI Cache; only used on the UI thread
    std::shared_ptr<AudioAnalysisData> uiDataCache;
};

using fnMidiBroadcast = std::function<void(const libremidi::message&)>;

#ifdef USE_LINK
struct LinkData
{
    double requestedTempo = 60.0;
    bool requestStart = false;
    bool requestStop = false;
    double quantum = 4.0;
    bool startStopSyncOn = true;
};
#endif

struct AudioContext
{
    bool m_initialized = false;
    bool m_isPlaying = true;
    bool m_audioValid = false;
    bool m_changedDeviceCombo = true;

    AudioCB m_fnCallback = nullptr;

    AudioChannelState inputState;
    AudioChannelState outputState;

    AudioSettings settings;
    AudioDeviceSettings audioDeviceSettings;

    // Audio analysis information. Processed outside of audio thread, consumed in UI,
    // so use system mutex, we don't need to spin
    std::map<ChannelId, std::shared_ptr<AudioAnalysis>> analysisChannels;
    AudioAnalysisSettings audioAnalysisSettings;

    std::atomic<uint64_t> analysisWriteGeneration = 0;
    std::atomic<uint64_t> analysisReadGeneration = 0;

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

    #ifdef USE_LINK
    std::atomic<std::chrono::microseconds> m_outputLatency;
    ableton::link::HostTimeFilter<ableton::link::platform::Clock> m_hostTimeFilter;
    std::mutex m_linkDataGuard;
    LinkData m_linkData;
    LinkData m_lockFreeLinkData;
    ableton::Link m_link = ableton::Link(20.0);
    #endif

    bool m_clicking = false;
    double m_clickFrequency;
    double m_clickTime = 0.0;
    int64_t m_lastClickBeat = 0;
    double m_tempo;
    int m_numPeers;

    // Total frames of audio sent, since start
    // These on the audio thread.
    uint64_t m_totalFrames = 0;
    std::chrono::microseconds m_frameInitTime;    // Start time for when we begin sending audio frames
    std::chrono::microseconds m_frameCurrentTime; // Current time of audio frame

    AudioSamples m_samples;

    // Midi
    moodycamel::ConcurrentQueue<libremidi::message> midi;

    // Master timer
    Zest::timer m_masterClock;

    // Clients
    std::vector<fnMidiBroadcast> midiClients;

    Zest::spin_mutex audioTickEnableMutex;

    std::vector<float> inputStreamOverride;
    uint32_t inputStreamIndex = 0;

    std::atomic<float> radioAgcPower = 0.0f;
    std::atomic<float> radioAgcPowerOut = 0.0f;
    std::atomic<float> radioOutAgcPower = 0.0f;
    std::atomic<float> radioOutAgcPowerOut = 0.0f;
    std::atomic<float> radioCompPower = 0.0f;
    std::atomic<float> radioCompPowerOut = 0.0f;
};

AudioContext& GetAudioContext();

void audio_add_settings_hooks();
bool audio_init(const AudioCB& fnCallback);
void audio_destroy();
void audio_show_link_gui();
void audio_show_settings_gui();

std::shared_ptr<AudioBundle> audio_get_bundle();
void audio_retire_bundle(std::shared_ptr<AudioBundle>& pBundle);

std::string audio_to_channel_name(ChannelId Id);
ChannelId audio_to_channel_id(uint32_t type, uint32_t channel);

void audio_add_midi_event(const libremidi::message& msg);

void audio_calculate_midi_timings(std::vector<libremidi::midi_track>& track, float ticksPerBeat);

#define CHECK_NOT_AUDIO_THREAD assert(std::this_thread::get_id() != ctx.threadId);

// Can't currently use this one since audio threads might be in a pool.  TLS?
#define CHECK_AUDIO_THREAD assert(std::this_thread::get_id() == ctx.threadId);

} // namespace Zing

#pragma once

#include <chrono>
#include <mutex>
#include <portaudio.h>
#include <complex>
#include <atomic>
#include <functional>

#include <concurrentqueue/concurrentqueue.h>
#include <zest/logger/logger.h>
#include <zest/memory/memory.h>

#include <zing/audio/audio_analysis_settings.h>
#include <zing/audio/audio_device_settings.h>

extern "C" {
#include <soundpipe/h/soundpipe.h>
}

#ifdef WIN32
#define LINK_PLATFORM_WINDOWS
#endif
#include <ableton/link/HostTimeFilter.hpp>
#include <ableton/platforms/Config.hpp>

//namespace mkiss
//{
#include <kiss_fft.h>
//}

union SDL_Event;

namespace Zing
{

struct AudioBundle
{
    std::vector<float> data;
};

struct AudioSettings
{
    std::atomic<bool> enableMetronome = false;
};

using AudioCB = std::function<void(const double beat, const double quantum, std::chrono::microseconds hostTime, void* pOutput, const void* pInput, uint32_t frameCount)>;

/*
struct DeviceInfo
{
    std::string fullName;
};
*/

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

static const uint32_t AnalysisSwapBuffers = 2;
struct AudioAnalysisData
{
    // Double buffer the data
    std::vector<float> spectrumBuckets[AnalysisSwapBuffers];
    std::vector<float> spectrum[AnalysisSwapBuffers];
    std::vector<float> audio[AnalysisSwapBuffers];
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

    PNL_CL_Memory<AudioAnalysisData, std::mutex> analysisData;
    
    // Bundles pending processing
    moodycamel::ConcurrentQueue<std::shared_ptr<AudioBundle>> processBundles;

};

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
    std::vector<std::shared_ptr<AudioAnalysis>> analysisChannels;
    AudioAnalysisSettings audioAnalysisSettings;

    std::atomic<uint64_t> analysisWriteGeneration = 0;
    std::atomic<uint64_t> analysisReadGeneration = 0;

    uint64_t m_sampleTime = 0;
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
    ableton::link::HostTimeFilter<ableton::link::platform::Clock> m_hostTimeFilter;
};

AudioContext& GetAudioContext();

inline glm::uvec4 Div(const glm::uvec4& val, uint32_t div)
{
    return glm::uvec4(val.x / div, val.y / div, val.z / div, val.w / div);
}

bool audio_init(const AudioCB& fnCallback);
void audio_destroy();
void audio_show_gui();

std::shared_ptr<AudioBundle> audio_get_bundle();
void audio_retire_bundle(std::shared_ptr<AudioBundle>& pBundle);

#define CHECK_NOT_AUDIO_THREAD assert(std::this_thread::get_id() != ctx.threadId);

// Can't currently use this one since audio threads might be in a pool.  TLS?
#define CHECK_AUDIO_THREAD assert(std::this_thread::get_id() == ctx.threadId);

} // namespace Zing

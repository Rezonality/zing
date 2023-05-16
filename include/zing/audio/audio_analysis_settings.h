#pragma once

#include <zest/file/toml_utils.h>
#include <zest/logger/logger.h>

#undef ERROR

namespace Zing
{

struct AudioAnalysisSettings
{
    uint32_t frames = 4096;
    uint32_t spectrumBuckets = 512;
    float spectrumSharpness = 8.0f;
    float blendFactor = 100.0f;
    bool blendFFT = true;
    bool logPartitions = true;
    bool filterFFT = true;
    bool normalizeAudio = false;
    bool removeFFTJitter = false;
    glm::uvec4 spectrumFrequencies = glm::uvec4(100, 500, 3000, 10000);
    glm::vec4 spectrumGains = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    float audioDecibelRange = 70.0f;
};

inline AudioAnalysisSettings audioanalysis_load_settings(const toml::table& settings)
{
    AudioAnalysisSettings analysisSettings;

    if (settings.empty())
        return analysisSettings;

    try
    {
        analysisSettings.frames = settings["frames"].value_or(analysisSettings.frames);
        analysisSettings.spectrumBuckets = settings["spectrum_buckets"].value_or(analysisSettings.spectrumBuckets);
        analysisSettings.spectrumSharpness = settings["spectrum_sharpness"].value_or(analysisSettings.spectrumSharpness);
        analysisSettings.blendFactor = settings["blend_factor"].value_or(analysisSettings.blendFactor);
        analysisSettings.blendFFT = settings["blend_fft"].value_or(analysisSettings.blendFFT);
        analysisSettings.logPartitions = settings["log_partitions"].value_or(analysisSettings.logPartitions);
        analysisSettings.filterFFT = settings["filter_fft"].value_or(analysisSettings.filterFFT);
        analysisSettings.removeFFTJitter = settings["dejitter_fft"].value_or(analysisSettings.removeFFTJitter);
        analysisSettings.spectrumFrequencies = toml_read_vec4(settings["spectrum_frequencies"], analysisSettings.spectrumFrequencies);
        analysisSettings.spectrumGains = toml_read_vec4(settings["spectrum_gains"], analysisSettings.spectrumGains);
        analysisSettings.audioDecibelRange = settings["audio_decibels"].value_or(analysisSettings.audioDecibelRange);
    }
    catch (std::exception& ex)
    {
        LOG(ERR, ex.what());
    }
    return analysisSettings;
}

inline toml::table audioanalysis_save_settings(const AudioAnalysisSettings& settings)
{
    auto freq = settings.spectrumFrequencies;
    auto gain = settings.spectrumGains;

    auto tab = toml::table{
        { "frames", int(settings.frames) },
        { "spectrum_buckets", int(settings.spectrumBuckets) },
        { "spectrum_sharpness", settings.spectrumSharpness },
        { "blend_factor", settings.blendFactor },
        { "blend_fft", settings.blendFFT },
        { "filter_fft", settings.filterFFT },
        { "dejitter_fft", settings.removeFFTJitter },
        { "log_partitions", settings.logPartitions },
        { "spectrum_frequencies", toml::array{ freq.x, freq.y, freq.z, freq.w } },
        { "spectrum_gains", toml::array{ gain.x, gain.y, gain.z, gain.w } },
        { "audio_decibels", settings.audioDecibelRange }
    };

    return tab;
}

inline void audio_analysis_validate_settings(AudioAnalysisSettings& settings)
{
    settings.frames = std::clamp(settings.frames, 64u, 4096u);
    settings.spectrumBuckets = std::clamp(settings.spectrumBuckets, 64u, settings.frames);
    settings.spectrumSharpness = std::clamp(settings.spectrumSharpness, 2.0f, 100.0f);
    settings.blendFactor = std::clamp(settings.blendFactor, 1.0f, 1000.0f);
    settings.spectrumFrequencies.x = std::clamp(settings.spectrumFrequencies.x, 0u, glm::uint(22000));
    settings.spectrumFrequencies.y = std::clamp(settings.spectrumFrequencies.y, settings.spectrumFrequencies.x, glm::uint(22000));
    settings.spectrumFrequencies.z = std::clamp(settings.spectrumFrequencies.z, settings.spectrumFrequencies.y, glm::uint(22000));
    settings.spectrumFrequencies.w = std::clamp(settings.spectrumFrequencies.w, settings.spectrumFrequencies.z, glm::uint(22000));
}

} // namespace Zing

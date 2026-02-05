#pragma once

#include <algorithm>
#include <cmath>

#include <zest/file/toml_utils.h>
#include <zest/common.h>
#include <zest/logger/logger.h>

#undef ERROR

namespace Zing
{

struct AudioAnalysisSettings
{
    uint32_t frames = 4096;
    uint32_t spectrumBuckets = 512;
    float blendFactor = 100.0f;
    bool blendFFT = true;
    bool filterFFT = true;
    bool normalizeAudio = false;
    bool removeFFTJitter = false;
    bool suppressDc = false;
    bool compEnabled = true;
    float compThresholdDb = -12.0f;
    float compRatio = 6.0f;
    float compAttack = 0.35f;
    float compRelease = 0.02f;
    glm::uvec4 spectrumFrequencies = glm::uvec4(100, 500, 3000, 10000);
    glm::vec4 spectrumGains = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    float audioDecibelRange = 110.0f;
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
        analysisSettings.blendFactor = settings["blend_factor"].value_or(analysisSettings.blendFactor);
        analysisSettings.blendFFT = settings["blend_fft"].value_or(analysisSettings.blendFFT);
        analysisSettings.filterFFT = settings["filter_fft"].value_or(analysisSettings.filterFFT);
        analysisSettings.removeFFTJitter = settings["dejitter_fft"].value_or(analysisSettings.removeFFTJitter);
        analysisSettings.suppressDc = settings["suppress_dc"].value_or(analysisSettings.suppressDc);
        analysisSettings.compEnabled = settings["comp_enabled"].value_or(analysisSettings.compEnabled);
        analysisSettings.compThresholdDb = settings["comp_threshold"].value_or(analysisSettings.compThresholdDb);
        analysisSettings.compRatio = settings["comp_ratio"].value_or(analysisSettings.compRatio);
        analysisSettings.compAttack = settings["comp_attack"].value_or(analysisSettings.compAttack);
        analysisSettings.compRelease = settings["comp_release"].value_or(analysisSettings.compRelease);
        analysisSettings.spectrumFrequencies = toml_read_vec4(settings["spectrum_frequencies"], analysisSettings.spectrumFrequencies);
        analysisSettings.spectrumGains = toml_read_vec4(settings["spectrum_gains"], analysisSettings.spectrumGains);
        analysisSettings.audioDecibelRange = settings["audio_decibels"].value_or(analysisSettings.audioDecibelRange);
    }
    catch (std::exception& ex)
    {
        UNUSED(ex);
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
        { "blend_factor", settings.blendFactor },
        { "blend_fft", settings.blendFFT },
        { "filter_fft", settings.filterFFT },
        { "dejitter_fft", settings.removeFFTJitter },
        { "suppress_dc", settings.suppressDc },
        { "comp_enabled", settings.compEnabled },
        { "comp_threshold", settings.compThresholdDb },
        { "comp_ratio", settings.compRatio },
        { "comp_attack", settings.compAttack },
        { "comp_release", settings.compRelease },
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
    settings.blendFactor = std::clamp(settings.blendFactor, 1.0f, 1000.0f);
    if (settings.compThresholdDb > 0.0f)
    {
        const float linear = std::max(settings.compThresholdDb, 1e-6f);
        settings.compThresholdDb = 20.0f * std::log10(linear);
    }
    settings.compThresholdDb = std::clamp(settings.compThresholdDb, -80.0f, 0.0f);
    settings.compRatio = std::clamp(settings.compRatio, 1.0f, 20.0f);
    settings.compAttack = std::clamp(settings.compAttack, 0.01f, 1.0f);
    settings.compRelease = std::clamp(settings.compRelease, 0.001f, 1.0f);
    settings.spectrumFrequencies.x = std::clamp(settings.spectrumFrequencies.x, 0u, glm::uint(22000));
    settings.spectrumFrequencies.y = std::clamp(settings.spectrumFrequencies.y, settings.spectrumFrequencies.x, glm::uint(22000));
    settings.spectrumFrequencies.z = std::clamp(settings.spectrumFrequencies.z, settings.spectrumFrequencies.y, glm::uint(22000));
    settings.spectrumFrequencies.w = std::clamp(settings.spectrumFrequencies.w, settings.spectrumFrequencies.z, glm::uint(22000));
}

} // namespace Zing

#pragma once

#include <zest/string/string_utils.h>

#include <tsf/tsf.h>

namespace Zing
{

struct SampleContainer
{
    union
    {
        tsf* soundFont;
    };
};

struct AudioSamples
{
    std::unordered_map<Zest::StringId, SampleContainer> samples;
    std::unordered_map<Zest::StringId, Zest::StringId> presetSamples;
};

bool samples_add(AudioSamples& samples, Zest::StringId id, const fs::path& path);
void samples_stop(AudioSamples& samples);
void samples_update_rate(AudioSamples& audioSamples);
void samples_render(AudioSamples& audioSamples, float* pOutput, uint32_t numSamples);
const SampleContainer* samples_find(AudioSamples& audioSamples);

} //namespace Zing
#include <zing/pch.h>

#include <zing/audio/audio.h>
#include <zing/audio/audio_samples.h>

using namespace Zest;

namespace Zing
{

bool samples_add(AudioSamples& audioSamples, Zest::StringId id, const fs::path& path)
{
    try
    {
        if (!fs::exists(path))
        {
            return false;
        }

        SampleContainer container;
        container.soundFont = tsf_load_filename(path.string().c_str());
        if (!container.soundFont)
        {
            return false;
        }

        audioSamples.samples[id] = container;

        int pre = tsf_get_presetcount(container.soundFont);
        for (int i = 0; i < pre; i++)
        {
            std::string name = tsf_get_presetname(container.soundFont, i);
            LOG(DBG, i << ":" << name);
            audioSamples.presetSamples[name] = id;
        }

        tsf_set_max_voices(container.soundFont, 256);

        return true;
    }
    catch (std::exception& ex)
    {
        UNUSED(ex);
        LOG(DBG, ex.what());
        return false;
    }
}

void samples_update_rate(AudioSamples& audioSamples)
{
    auto& ctx = GetAudioContext();
    if (ctx.outputState.channelCount == 0)
    {
        return;
    }

    for (auto& [id, container] : audioSamples.samples)
    {
        if (container.soundFont)
        {
            tsf_set_output(container.soundFont, ctx.outputState.channelCount == 2 ? TSF_STEREO_INTERLEAVED : TSF_MONO, ctx.outputState.sampleRate, 0.0f);
        }
    }
}

void samples_stop(AudioSamples& audioSamples)
{
    auto& ctx = GetAudioContext();
    for (auto& [id, container] : audioSamples.samples)
    {
        if (container.soundFont)
        {
            // TODO: Is this correct?
            // Do we care?
            tsf_channel_note_off_all(container.soundFont, 0);
            tsf_channel_note_off_all(container.soundFont, 1);
        }
    }
}

void samples_render(AudioSamples& audioSamples, float* pOutput, uint32_t numSamples)
{
    auto& ctx = GetAudioContext();
    if (ctx.outputState.channelCount == 0 || pOutput == nullptr || numSamples == 0)
    {
        return;
    }
    for (auto& [id, container] : audioSamples.samples)
    {
        if (container.soundFont)
        {
            tsf_render_float(container.soundFont, (float*)pOutput, int(numSamples), 1);
        }
    }
}

const SampleContainer* samples_find(AudioSamples& audioSamples)
{
    auto& ctx = GetAudioContext();
    for (auto& [id, container] : audioSamples.samples)
    {
        if (container.soundFont)
        {
            return &container;
        }
    }
    return nullptr;
}

} //namespace Zing
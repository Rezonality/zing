#include "pch.h"

#include <zest/settings/settings.h>

#include <zing/audio/audio.h>
#include <zing/audio/midi.h>

#include <config_zing_app.h>

#include <tsf/tsf.h>

using namespace Zing;
using namespace std::chrono;

namespace {

bool playNote = false;
sp_osc* osc = nullptr;
sp_ftbl* ft = nullptr;
sp_phaser *phs = nullptr;

std::atomic<bool> g_noteOn = false;
std::atomic<bool> g_noteOff = false;
}

void demo_init()
{
    auto& ctx = GetAudioContext();

    samples_add(ctx.m_samples, "GM", Zest::runtree_find_path("samples/sf2/LiveHQ.sf2"));

    midi_probe();
    midi_load(Zest::runtree_find_path("samples/midi/demo-1.mid"));

    audio_init([=](const std::chrono::microseconds hostTime, void* pOutput, std::size_t numSamples) {
        auto& ctx = GetAudioContext();

        if (g_noteOn)
        {
            for (auto& [id, container] : ctx.m_samples.samples)
            {
                auto tsf = container.soundFont;
                if (tsf)
                {
                    tsf_note_on(tsf, 0, 48, 0.5f);
                    tsf_note_on(tsf, 24, 48, 1.0f);
                    tsf_note_on(tsf, 0, 52, 0.5f);
                    tsf_note_on(tsf, 24, 52, 1.0f);
                }
            }
            g_noteOn = false;
        }
        if (g_noteOff)
        {
            for (auto& [id, container] : ctx.m_samples.samples)
            {
                auto tsf = container.soundFont;
                if (tsf)
                {
                    tsf_note_off(tsf, 0, 48);
                    tsf_note_off(tsf, 24, 48);
                    tsf_note_off(tsf, 0, 52);
                    tsf_note_off(tsf, 24, 52);
                }
            }
            g_noteOff = false;
        }

        /*
        if (!osc)
        {
            sp_ftbl_create(ctx.pSP, &ft, 8192);
            sp_osc_create(&osc);

            sp_gen_triangle(ctx.pSP, ft);
            sp_osc_init(ctx.pSP, osc, ft, 0);
            osc->freq = 500;
    
            sp_phaser_create(&phs);
            sp_phaser_init(ctx.pSP, phs);
        }

        auto pOut = (float*)pOutput;
        for (uint32_t i = 0; i < numSamples; i++)
        {
            float out[2] = { 0.0f, 0.0f };

            if (playNote)
            {
                sp_osc_compute(ctx.pSP, osc, &out[0], &out[0]);
                sp_phaser_compute(ctx.pSP, phs, &out[0], &out[0], &out[0], &out[1]);
            }

            for (uint32_t ch = 0; ch < ctx.outputState.channelCount; ch++)
            {
                *pOut++ += out[ch];
            }
        }
        */

        samples_render(ctx.m_samples, (float*)pOutput, int(numSamples));
    });
}

void demo_draw_analysis()
{
    auto& audioContext = GetAudioContext();

    size_t bufferWidth = 512; // default width if no data
    const auto Channels = audioContext.analysisChannels.size();
    const auto BufferTypes = 2; // Spectrum + Audio
    const auto BufferHeight = Channels * BufferTypes;

    for (int channel = 0; channel < Channels; channel++)
    {
        auto& analysis = audioContext.analysisChannels[channel];

        ConsumerMemLock memLock(analysis->analysisData);
        auto& processData = memLock.Data();
        auto currentBuffer = 1 - processData.currentBuffer;

        auto& spectrumBuckets = processData.spectrumBuckets[currentBuffer];
        auto& audio = processData.audio[currentBuffer];

        if (!spectrumBuckets.empty())
        {
            ImVec2 plotSize(300, 100);
            ImGui::PlotLines(fmt::format("Spectrum: {}", channel).c_str(), &spectrumBuckets[0], static_cast<int>(spectrumBuckets.size()), 0, NULL, 0.0f, 1.0f, plotSize);
            ImGui::PlotLines(fmt::format("Audio: {}", channel).c_str(), &audio[0], static_cast<int>(audio.size()), 0, NULL, -1.0f, 1.0f, plotSize);
        }
    }
}

void demo_draw()
{

    // Settings
    Zest::GlobalSettingsManager::Instance().DrawGUI("Settings");

    // Profiler
    ImGui::Begin("Profiler");
    static bool open = true;
    Zest::Profiler::ShowProfile(&open);
    ImGui::End();

    // Audio and link
    ImGui::Begin("Audio Settings");
    audio_show_settings_gui();
    ImGui::End();

    auto& ctx = GetAudioContext();
    ImGui::Begin("Audio");
    
    ImGui::SeparatorText("Link");
    audio_show_link_gui();

    ImGui::SeparatorText("Test");
    ImGui::BeginDisabled(ctx.outputState.channelCount == 0 ? true : false);
    if (ImGui::Checkbox("Tone", &playNote))
    {
        if (playNote)
        {
            g_noteOn = true;
        }
        else
        {
            g_noteOff = true;
        }
    }
  
    bool metro = ctx.m_playMetronome;
    if (ImGui::Checkbox("Metronome", &metro))
    {
        ctx.m_playMetronome = metro;
    }
    ImGui::EndDisabled();

    ImGui::SeparatorText("Analysis");
    demo_draw_analysis();

    ImGui::End();
}

void demo_cleanup()
{
    // Get the settings
    audio_destroy();
}


#include <filesystem>
#include <fmt/format.h>
#include <memory>

#include <zest/math/imgui_glm.h>
#include <zest/settings/settings.h>
#include <zest/time/profiler.h>
#include <zest/time/timer.h>
#include <zest/file/runtree.h>
#include <zest/file/file.h>

#include <zing/audio/audio.h>

#include <config_zing_app.h>

using namespace Zing;
using namespace std::chrono;
namespace fs = std::filesystem;

namespace {

bool playNote = false;
sp_osc* osc = nullptr;
sp_ftbl* ft = nullptr;
sp_phaser *phs = nullptr;

}

void demo_init()
{
    auto& ctx = GetAudioContext();

    audio_init([=](const std::chrono::microseconds hostTime, void* pOutput, std::size_t numSamples) {
        auto& ctx = GetAudioContext();

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
    ImGui::Checkbox("Tone", &playNote);
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


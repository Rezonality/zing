#include <filesystem>
#include <fmt/format.h>
#include <imgui.h>
#include <memory>
#include <zing/audio/audio.h>
extern "C" {
#include <soundpipe/h/soundpipe.h>
}

#include <config_app.h>
using namespace Zing;
namespace fs = std::filesystem;

namespace {
}

void demo_init()
{
    audio_init(nullptr);
}

void demo_draw_analysis()
{
    auto& audioContext = GetAudioContext();

    size_t bufferWidth = 512; // default width if no data
    const auto Channels = std::max(audioContext.analysisChannels.size(), size_t(1));
    const auto BufferTypes = 2; // Spectrum + Audio
    const auto BufferHeight = Channels * BufferTypes;

    ImGui::Begin("Audio Analysis");
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
            ImVec2 plotSize(400, 200);
            ImGui::PlotLines(fmt::format("Spectrum: {}", channel).c_str(), &spectrumBuckets[0], static_cast<int>(spectrumBuckets.size()), 0, NULL, 0.0f, 1.0f, plotSize);
            ImGui::PlotLines(fmt::format("Audio: {}", channel).c_str(), &audio[0], static_cast<int>(audio.size()), 0, NULL, -1.0f, 1.0f, plotSize);
        }
    }
    ImGui::End();
}

void demo_draw()
{
    demo_draw_analysis();

    ImGui::Begin("Audio Settings");
    audio_show_gui();
    ImGui::End();
}

void demo_cleanup()
{
    audio_destroy();
}

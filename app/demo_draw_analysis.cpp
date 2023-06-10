#include "demo.h"
#include "pch.h"

#include <deque>

#include <zest/time/timer.h>
#include <zest/ui/colors.h>

#include <zing/audio/audio.h>

using namespace Zing;
using namespace Zest;
using namespace std::chrono;
using namespace libremidi;

void demo_draw_analysis()
{
    PROFILE_SCOPE(demo_draw_analysis)
    auto& ctx = GetAudioContext();

    const size_t bufferWidth = 512;   // default width if no data
    const auto BufferTypes = 2; // Spectrum + Audio
    const auto BufferHeight = ctx.analysisChannels.size() * BufferTypes;

    for (uint32_t i = 0; i < 2; i++)
    {
        for (auto [Id, pAnalysis] : ctx.analysisChannels)
        {
            std::shared_ptr<AudioAnalysisData> spNewData;
            while (pAnalysis->analysisData.try_dequeue(spNewData))
            {
                if (pAnalysis->uiDataCache)
                {
                    pAnalysis->analysisDataCache.enqueue(pAnalysis->uiDataCache);
                }
                pAnalysis->uiDataCache = spNewData;
            }

            if (!pAnalysis->uiDataCache)
            {
                continue;
            }

            const auto& spectrumBuckets = pAnalysis->uiDataCache->spectrumBuckets;
            const auto& audio = pAnalysis->uiDataCache->audio;

            if (!spectrumBuckets.empty())
            {
                ImVec2 plotSize(300, 100);
                if (i == 0)
                {
                    ImGui::PlotLines(fmt::format("Spectrum: {}", audio_to_channel_name(Id)).c_str(), &spectrumBuckets[0], static_cast<int>(spectrumBuckets.size() / 2.5), 0, NULL, 0.0f, 1.0f, plotSize);
                }
                else
                {
                    ImGui::PlotLines(fmt::format("Audio: {}", audio_to_channel_name(Id)).c_str(), &audio[0], static_cast<int>(audio.size()), 0, NULL, -1.0f, 1.0f, plotSize);
                }
            }
        }
    }
}

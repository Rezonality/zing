// draw_waterfall.cpp
#include <zing/pch.h>

#include <zing/audio/audio.h>
#include <zing/audio/waterfall.h>

using namespace Zing;
using namespace Zest;

void draw_waterfall()
{
    PROFILE_SCOPE(draw_waterfall)
    auto& ctx = GetAudioContext();
    auto& wf = Waterfall_Get();

    for (auto [Id, pAnalysis] : ctx.analysisChannels)
    {
        if (Id.first != Channel_In || Id.second != 0)
        {
            // Only show first input channel
            continue;
        }

        if (!pAnalysis->uiDataCache)
        {
            continue;
        }

        const auto& spectrumBuckets = pAnalysis->uiDataCache->spectrumBuckets;
        if (!spectrumBuckets.empty())
        {
            auto bucketCount = spectrumBuckets.size();
            auto sampleCount = ctx.audioDeviceSettings.sampleRate * 0.5f;
            sampleCount /= float(spectrumBuckets.size());

            auto fallRows = 50;
            if (wf.bins != int(bucketCount))
            {
                Waterfall_Init(wf, int(bucketCount), fallRows);
            }


            Waterfall_AccumulateMag(wf, spectrumBuckets.data(), int(bucketCount));

            Waterfall_DrawPlot(wf, "Waterfall", float(bucketCount * sampleCount), ImVec2(-1, float(fallRows * 10)));
        }
    }
}

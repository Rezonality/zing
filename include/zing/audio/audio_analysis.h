#pragma once

#include <atomic>
#include <zing/audio/audio.h>
#include <concurrentqueue/concurrentqueue.h>

namespace Zing
{

void audio_analysis_destroy_all();
void audio_analysis_create_all();

bool audio_analysis_start(AudioAnalysis& analyis, const AudioChannelState& state);
void audio_analysis_stop(AudioAnalysis& analyis);
void audio_analysis_update(AudioAnalysis& analysis, AudioBundle& bundle);

uint32_t audio_analysis_read_index(AudioAnalysisData& analysis);
uint32_t audio_analysis_write_index(AudioAnalysisData& analysis);

} // namespace Zing

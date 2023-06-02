#include "pch.h"

#include <deque>

#include <zest/settings/settings.h>
#include <zest/ui/colors.h>

#include <zing/audio/audio.h>
#include <zing/audio/audio_analysis.h>
#include <zing/audio/midi.h>

#include <config_zing_app.h>

using namespace Zing;
using namespace std::chrono;

namespace
{

std::deque<tml_message> showMidi;
static double g_Msec = 0.0; //current playback time

} //namespace

void demo_init()
{
    auto& ctx = GetAudioContext();

    // Temporary load here of samples
    samples_add(ctx.m_samples, "GM", Zest::runtree_find_path("samples/sf2/LiveHQ.sf2"));
    auto tsf = ctx.m_samples.samples["GM"].soundFont;

    tml_message* pMidi = tml_load_filename(Zest::runtree_find_path("samples/sf2/demo-1.mid").string().c_str());

    ctx.midiClients.push_back([](const tml_message& msg) {
        showMidi.push_back(msg);
    });

    // Queue the midi
    while (pMidi)
    {
        audio_add_midi_event(*pMidi);
        pMidi = pMidi->next;
    }

    audio_init([=](const std::chrono::microseconds hostTime, void* pOutput, std::size_t numSamples) {
        auto& ctx = GetAudioContext();

        if (!tsf)
        {
            return;
        }
        static tml_message msg;
        static bool pendingMessage = false;
        static const uint64_t blockSize = 64;

        uint64_t currentBlockSize = 0;
        assert(numSamples % blockSize == 0);

        auto pOut = (float*)pOutput;

        auto emptyTheBlock = [&]() {
            if (currentBlockSize > 0)
            {
                samples_render(ctx.m_samples, pOut, int(currentBlockSize));
                currentBlockSize = 0;
                pOut += (blockSize * ctx.outputState.channelCount);
            }
        };

        for (uint32_t sample = 0; sample < numSamples; sample++)
        {
            if (!pendingMessage)
            {
                if (ctx.midi.try_dequeue(msg))
                {
                    pendingMessage = true;
                }
            }

            if (g_Msec >= msg.time)
            {
                switch (msg.type)
                {
                    case TML_PROGRAM_CHANGE: //channel program (preset) change (special handling for 10th MIDI channel with drums)
                        tsf_channel_set_presetnumber(tsf, msg.channel, msg.program, (msg.channel == 9));
                        break;
                    case TML_NOTE_ON: //play a note
                        tsf_channel_note_on(tsf, msg.channel, msg.key, msg.velocity / 127.0f);
                        break;
                    case TML_NOTE_OFF: //stop a note
                        tsf_channel_note_off(tsf, msg.channel, msg.key);
                        break;
                    case TML_PITCH_BEND: //pitch wheel modification
                        tsf_channel_set_pitchwheel(tsf, msg.channel, msg.pitch_bend);
                        break;
                    case TML_CONTROL_CHANGE: //MIDI controller messages
                        tsf_channel_midi_control(tsf, msg.channel, msg.control, msg.control_value);
                        break;
                }
                pendingMessage = false;
            }

            g_Msec += 1000.0 / ctx.outputState.sampleRate;
            currentBlockSize++;

            if (currentBlockSize == blockSize)
            {
                emptyTheBlock();
            }
        }

        emptyTheBlock();
    });
}

struct NoteKey
{
    int channel;
    int key;
};

bool operator==(const NoteKey& lhs, const NoteKey& rhs)
{
    return (lhs.channel == rhs.channel) && (lhs.key == rhs.key);
}

template <>
struct std::hash<NoteKey>
{
    std::size_t operator()(NoteKey const& s) const noexcept
    {
        return std::hash<size_t>{}(s.channel) ^ std::hash<size_t>{}(s.key << 8); // or use boost::hash_combine
    }
};

void demo_draw_midi()
{
    PROFILE_SCOPE(demo_draw_midi);
    float startTime = float(g_Msec) - 1000.0f;
    float endTime = float(g_Msec) + 2000.0f;
    float currentTime = float(g_Msec);

    struct DisplayNote
    {
        DisplayNote(float _start, int _key)
            : start(_start)
            , end(std::numeric_limits<float>::max())
            , key(_key)
        {
        }
        float start;
        float end;
        bool finished = false;
        int key;
    };

    static std::unordered_map<NoteKey, std::deque<DisplayNote>> activeNotes; // Map note key to start time
    while (!showMidi.empty())
    {
        const auto& msg = showMidi.front();

        NoteKey key = NoteKey{msg.channel, msg.key};

        float time = float(msg.time);

        auto itrFound = activeNotes.find(key);
        if (msg.type == TML_NOTE_ON)
        {
            // This note starts after the end of the frame, ignore for now
            // We'll come back later
            if (msg.time >= endTime)
            {
                break;
            }

            if (!activeNotes[key].empty() && (!activeNotes[key].back().finished))
            {
                // Skip
                LOG(DBG, "Double note on...");
            }
            else
            {
                // Add the new one
                activeNotes[key].push_back(DisplayNote(time, key.key));
            }
        }
        else if (msg.type == TML_NOTE_OFF)
        {
            // Update the time
            auto itrFound = activeNotes.find(key);
            if (itrFound != activeNotes.end())
            {
                // Update the time range of the last note
                itrFound->second.back().end = time;
                itrFound->second.back().finished = true;
            }
        }
        showMidi.pop_front();
    }

    struct ChannelRange
    {
        float low = std::numeric_limits<float>::max();
        float high = std::numeric_limits<float>::min();
        float step = 0;
        bool empty = true;
        int shrinkCount = 0;
    };

    std::map<int, ChannelRange> channelRanges;

    for (auto& [channel, range] : channelRanges)
    {
        range.empty = true;
    }

    // Now cull
    auto itr = activeNotes.begin();
    while (itr != activeNotes.end())
    {
        auto& displayNoteQueue = itr->second;
        auto itrDisplayNote = displayNoteQueue.begin();
        while (itrDisplayNote != displayNoteQueue.end())
        {
            // Remove this one
            if (itrDisplayNote->end < startTime)
            {
                displayNoteQueue.pop_front();
                itrDisplayNote = displayNoteQueue.begin();
            }
            else
            {
                channelRanges[itr->first.channel].low = std::min(channelRanges[itr->first.channel].low, float(itrDisplayNote->key));
                channelRanges[itr->first.channel].high = std::max(channelRanges[itr->first.channel].high, float(itrDisplayNote->key));
                channelRanges[itr->first.channel].empty = false;

                itrDisplayNote++;
            }
        }

        // Still have active notes
        if (displayNoteQueue.empty())
        {
            itr = activeNotes.erase(itr);
        }
        else
        {
            itr++;
        }
    };

    float step = 0.0f;
    for (auto& [channel, range] : channelRanges)
    {
        if (range.empty)
        {
            range.shrinkCount++;
            if (range.shrinkCount > 100)
            {
                range.low = 0.0f;
                range.high = 0.0f;
                range.shrinkCount = 0;
            }
        }

        range.step = step;
        step += (range.high - range.low) + 2;
    }

    if (ImGui::Begin("Midi"))
    {
        float timeRange = endTime - startTime;

        auto regionSize = ImGui::GetContentRegionAvail();
        glm::vec2 regionMin(ImGui::GetCursorScreenPos());
        glm::vec2 regionMax(regionMin.x + regionSize.x, regionMin.y + regionSize.y);

        float xPos = regionMin.x;
        float yPos = regionMin.y;

        xPos += (1000.0f / timeRange) * regionSize.x;
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(xPos, yPos),
            ImVec2(xPos + 1, yPos + regionSize.y),
            0xFFFFFFFF);

        // Iterate over the MIDI events and display them within the time range
        for (auto& [noteKey, displayNotes] : activeNotes)
        {
            for (auto& displayNote : displayNotes)
            {
                auto& range = channelRanges[noteKey.channel];

                auto noteBegin = std::clamp(displayNote.start, startTime, endTime);
                auto noteEnd = std::clamp(displayNote.end, startTime, endTime);

                // Calculate the position of the event within the window
                float xPos = regionMin.x + (((noteBegin - startTime) / timeRange) * regionSize.x);
                float yPos = regionMax.y - ((range.step + (noteKey.key - range.low)) * 5.0f);

                // Set the color based on the event type
                auto col = Zest::colors_get_default(noteKey.channel);
                float barWidth = ((noteEnd - noteBegin) / timeRange) * regionSize.x;

                if (noteBegin <= currentTime && noteEnd >= currentTime)
                {
                    col.w = 1.0f;
                }
                else
                {
                    col.w = 0.5f;
                }
                if ((xPos >= regionMin.x) && (barWidth <= regionSize.x))
                {
                    auto color = glm::packUnorm4x8(col);
                    ImGui::GetWindowDrawList()->AddRectFilled(
                        ImVec2(xPos, yPos),
                        ImVec2(xPos + barWidth, yPos + 8.0f), // * (msg.velocity / 127.0f)),
                        color);
                }
                else
                {
                    assert(!"WTF");
                }
            }

            // Draw a vertical line at the position of the event
            /* ImGui::GetWindowDrawList()->AddLine(
                ImVec2(xPos, yPos),
                ImVec2(xPos, yPos + 8.0f),
                color);
                */
        }
    }
    ImGui::End();
}

void demo_draw_analysis()
{
    PROFILE_SCOPE(demo_draw_analysis)
    auto& audioContext = GetAudioContext();

    size_t bufferWidth = 512;   // default width if no data
    const auto Channels = audioContext.analysisChannels.size();
    const auto BufferTypes = 2; // Spectrum + Audio
    const auto BufferHeight = Channels * BufferTypes;

    static AudioAnalysisData currentData;
    for (int channel = 0; channel < Channels; channel++)
    {
        auto& analysis = audioContext.analysisChannels[channel];

        std::shared_ptr<AudioAnalysisData> spNewData;
        while (analysis->analysisData.try_dequeue(spNewData))
        {
            currentData = *spNewData;
            analysis->analysisDataCache.enqueue(spNewData);
        }

        auto& spectrumBuckets = currentData.spectrumBuckets;
        auto& audio = currentData.audio;

        if (!spectrumBuckets.empty())
        {
            ImVec2 plotSize(300, 100);
            ImGui::PlotLines(fmt::format("Spectrum: {}", audio_to_channel_name(channel)).c_str(), &spectrumBuckets[0], static_cast<int>(spectrumBuckets.size() / 2.5), 0, NULL, 0.0f, 1.0f, plotSize);
            ImGui::PlotLines(fmt::format("Audio: {}", audio_to_channel_name(channel)).c_str(), &audio[0], static_cast<int>(audio.size()), 0, NULL, -1.0f, 1.0f, plotSize);
        }
    }
}

void demo_draw()
{
    PROFILE_SCOPE(demo_draw)

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
    /*
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
    */

    bool metro = ctx.m_playMetronome;
    if (ImGui::Checkbox("Metronome", &metro))
    {
        ctx.m_playMetronome = metro;
    }
    ImGui::EndDisabled();

    ImGui::SeparatorText("Analysis");
    demo_draw_analysis();

    ImGui::End();

    demo_draw_midi();
}

void demo_cleanup()
{
    // Get the settings
    audio_destroy();
}

// Old/Kept
/*

    //midi_probe();
    //midi_load(Zest::runtree_find_path("samples/midi/demo-1.mid"));

sp_osc* osc = nullptr;
sp_ftbl* ft = nullptr;
sp_phaser* phs = nullptr;
bool playNote = false;

std::atomic<bool> g_noteOn = false;
std::atomic<bool> g_noteOff = false;
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
            */

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

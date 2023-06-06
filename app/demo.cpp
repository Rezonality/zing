#include "pch.h"

#include <deque>

#include <zest/settings/settings.h>
#include <zest/ui/colors.h>
#include <zest/file/file.h>

#include <zing/audio/audio.h>
#include <zing/audio/audio_analysis.h>
#include <zing/audio/midi.h>

#include <config_zing_app.h>

#include <libremidi/reader.hpp>

using namespace Zing;
using namespace Zest;
using namespace std::chrono;
using namespace libremidi;

namespace
{

std::deque<libremidi::message> showMidi;

} //namespace

void demo_init()
{
    auto& ctx = GetAudioContext();

    // Temporary load here of samples
    // Put a nicer/bigger soundfont here to hear a better rendition.
    //samples_add(ctx.m_samples, "GM", Zest::runtree_find_path("samples/sf2/LiveHQ.sf2"));
    //samples_add(ctx.m_samples, "GM", Zest::runtree_find_path("samples/sf2/TimbresOfHeaven.sf2"));
    samples_add(ctx.m_samples, "GM", Zest::runtree_find_path("samples/sf2/233_poprockbank.sf2"));
    auto tsf = ctx.m_samples.samples["GM"].soundFont;

    auto midiFile = file_read(Zest::runtree_find_path("samples/midi/demo-1.mid"));

    auto pReader = std::make_shared<libremidi::reader>(true);
    auto midiRes = pReader->parse((uint8_t*)midiFile.c_str(), midiFile.size());
    if (midiRes != reader::validated)
    {
        pReader.reset();
    }

    ctx.midiClients.push_back([](const libremidi::message& msg) {
        showMidi.push_back(msg);
    });

    audio_init([=](const std::chrono::microseconds hostTime, void* pOutput, std::size_t numSamples) {
        static bool init = false;

        if (!init)
        {
            auto& ctx = GetAudioContext();
            auto time = timer_to_ms(timer_get_elapsed(ctx.m_masterClock));
            time += 3000.0;

            if (pReader)
            {
                // Queue the midi; just for the demo, reset the time.
                for (const auto& track : pReader->tracks)
                {
                    for (const auto& msg : track)
                    {
                        auto m = msg.m;
                        m.timestamp = msg.tick + time;
                        audio_add_midi_event(m);
                    }
                }
            }
            init = true;
        }
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

    auto& ctx = GetAudioContext();
    auto time = timer_to_ms(timer_get_elapsed(ctx.m_masterClock));
    auto startTime = time - 1000.0f;
    auto endTime = startTime + 3000.0f;

    struct DisplayNote
    {
        DisplayNote(double _start, int _key)
            : start(_start)
            , end(std::numeric_limits<double>::max())
            , key(_key)
        {
        }
        double start;
        double end;
        bool finished = false;
        int key;
    };

    static std::unordered_map<NoteKey, std::deque<DisplayNote>> activeNotes; // Map note key to start time
    while (!showMidi.empty())
    {
        const auto& msg = showMidi.front();

        if (msg.is_note_on_or_off())
        {
            NoteKey key = NoteKey{msg.get_channel(), msg[1]};

            auto itrFound = activeNotes.find(key);
            if (msg.get_message_type() == libremidi::message_type::NOTE_ON)
            {

                // This note starts after the end of the frame, ignore for now
                // We'll come back later
                if (msg.timestamp >= endTime)
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
                    activeNotes[key].push_back(DisplayNote(msg.timestamp, key.key));
                    LOG(DBG, "Note On: " << key.key << ", time: " << msg.timestamp);
                }
            }
            else if (msg.get_message_type() == libremidi::message_type::NOTE_OFF)
            {
                // Update the time
                auto itrFound = activeNotes.find(key);
                if (itrFound != activeNotes.end())
                {
                    // Update the time range of the last note
                    itrFound->second.back().end = msg.timestamp;
                    itrFound->second.back().finished = true;
                    
                    LOG(DBG, "Note Off: " << key.key << ", time: " << msg.timestamp);
                }
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
        auto timeRange = (endTime - startTime);

        auto regionSize = ImGui::GetContentRegionAvail();
        glm::vec2 regionMin(ImGui::GetCursorScreenPos());
        glm::vec2 regionMax(regionMin.x + regionSize.x, regionMin.y + regionSize.y);

        double xPos = regionMin.x;
        float yPos = regionMin.y;

        xPos += (1000.0 / timeRange) * regionSize.x;
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(float(xPos), yPos),
            ImVec2(float(xPos) + 1.0f, yPos + regionSize.y),
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
                auto xPos = regionMin.x + (((noteBegin - startTime) / timeRange) * regionSize.x);
                auto yPos = regionMax.y - ((range.step + (noteKey.key - range.low)) * 5.0f);

                // Set the color based on the event type
                auto col = Zest::colors_get_default(noteKey.channel);
                auto barWidth = ((noteEnd - noteBegin) / timeRange) * regionSize.x;

                if (noteBegin <= time && noteEnd >= time)
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
                        ImVec2(float(xPos), yPos),
                        ImVec2(float(xPos + barWidth), yPos + 8.0f), // * (msg.velocity / 127.0f)),
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

    auto& ctx = GetAudioContext();
    auto hostTime = duration_cast<milliseconds>(ctx.m_link.clock().micros());

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

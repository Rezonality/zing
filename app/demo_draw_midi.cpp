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

namespace
{
std::deque<libremidi::message> showMidi;

struct NoteKey
{
    int channel;
    int key;
};
bool operator==(const NoteKey& lhs, const NoteKey& rhs)
{
    return (lhs.channel == rhs.channel) && (lhs.key == rhs.key);
}
} //namespace

template <>
struct std::hash<NoteKey>
{
    std::size_t operator()(NoteKey const& s) const noexcept
    {
        return std::hash<size_t>{}(s.channel) ^ std::hash<size_t>{}(s.key << 8); // or use boost::hash_combine
    }
};

void demo_draw_midi_init()
{
    auto& ctx = GetAudioContext();

    // Add our midi display as a midi client
    ctx.midiClients.push_back([](const libremidi::message& msg) {
        showMidi.push_back(msg);
    });
}

// This draws a simple midi timeline.
// It tries to fit everything into a small space, while seperating the instruments into their own vertical spaces.
// This is just for debug/temporary.  A nice visualization will be forthcoming.
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
        if (msg.size() == 0)
        {
            showMidi.pop_front();
            continue;
        }

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
                    //LOG(DBG, "Note On: " << key.key << ", time: " << msg.timestamp);
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

                    //LOG(DBG, "Note Off: " << key.key << ", time: " << msg.timestamp);
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
    }
}

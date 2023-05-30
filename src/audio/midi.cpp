#include <zing/pch.h>
#include <zest/file/file.h>

#include <libremidi/libremidi.hpp>
#include <libremidi/reader.hpp>

using namespace libremidi;
namespace Zing
{

bool midi_load(const fs::path& path)
{
    auto bytes = Zest::file_read(path);

    // Initialize our reader object
    libremidi::reader r{true};

    // Parse
    libremidi::reader::parse_result result = r.parse((uint8_t*)bytes.c_str(), bytes.size());

    switch (result)
    {
        case libremidi::reader::validated:
            LOG(DBG, "Parsing validated");
            // Parsing has succeeded, all the input data is correct MIDI.
            break;

        case libremidi::reader::complete:
            LOG(DBG, "Parsing complete");
            // All the input data is parsed but the MIDI file was not necessarily strict SMF
            // (e.g. there are empty tracks or tracks without END OF TRACK)
            break;

        case libremidi::reader::incomplete:
            LOG(DBG, "Parsing incomplete");
            // Not all the input could be parsed. For instance a track could not be read.
            break;

        case libremidi::reader::invalid:
            LOG(DBG, "Parsing invalid");
            // Nothing could be parsed, this is not MIDI data or we do not support it yet.
            return false;
    }

    if (result != libremidi::reader::invalid)
    {
        for (const auto& track : r.tracks)
        {
            LOG(DBG, "New track");
            for (const libremidi::track_event& event : track)
            {
                LOG(DBG, "Event at " << event.tick << " : ");
                if (event.m.is_meta_event())
                {
                    switch (event.m.get_meta_event_type())
                    {
                        case meta_event_type::SEQUENCE_NUMBER:
                            LOG(DBG, "Meta event: SEQUENCE_NUMBER");
                            break;
                        case meta_event_type::TEXT:
                            LOG(DBG, "Meta event: TEXT");
                            break;
                        case meta_event_type::COPYRIGHT:
                            LOG(DBG, "Meta event: COPY");
                            break;
                        case meta_event_type::TRACK_NAME:
                            LOG(DBG, "Meta event: TRACK_NAME: " << std::string((char*)&event.m.bytes[3], ((char*)&event.m.bytes[event.m.bytes.size() - 1]) + 1));
                            break;
                        case meta_event_type::INSTRUMENT:
                            LOG(DBG, "Meta event: INSTRUMENT");
                            break;
                        case meta_event_type::LYRIC:
                            LOG(DBG, "Meta event: LYRIC");
                            break;
                        case meta_event_type::MARKER:
                            LOG(DBG, "Meta event: MARKER");
                            break;
                        case meta_event_type::CUE:
                            LOG(DBG, "Meta event: CUE");
                            break;
                        case meta_event_type::PATCH_NAME:
                            LOG(DBG, "Meta event: PATCH_NAME");
                            break;
                        case meta_event_type::DEVICE_NAME:
                            LOG(DBG, "Meta event: DEVICE_NAME");
                            break;
                        case meta_event_type::CHANNEL_PREFIX:
                            LOG(DBG, "Meta event: CHANNEL_PREFIX");
                            break;
                        case meta_event_type::MIDI_PORT:
                            LOG(DBG, "Meta event: MIDI_PORT");
                            break;
                        case meta_event_type::END_OF_TRACK:
                            LOG(DBG, "Meta event: END_OF_TRACK");
                            break;
                        case meta_event_type::TEMPO_CHANGE:
                            LOG(DBG, "Meta event: TEMPO_CHANGE");
                            break;
                        case meta_event_type::SMPTE_OFFSET:
                            LOG(DBG, "Meta event: SMPTE_OFFSET");
                            break;
                        case meta_event_type::TIME_SIGNATURE:
                            LOG(DBG, "Meta event: TIME_SIGNATURE");
                            break;
                        case meta_event_type::KEY_SIGNATURE:
                            LOG(DBG, "Meta event: KEY_SIGNATURE");
                            break;
                        case meta_event_type::PROPRIETARY:
                            LOG(DBG, "Meta event: PROPRIETARY");
                            break;
                    }
                }
                else
                {
                    switch (event.m.get_message_type())
                    {
                        case libremidi::message_type::NOTE_ON:
                            LOG(DBG, "Note ON: "
                                         << "channel " << event.m.get_channel() << ' ' << "note " << (int)event.m.bytes[1] << ' ' << "velocity " << (int)event.m.bytes[2] << ' ');
                            break;
                        case libremidi::message_type::NOTE_OFF:
                            LOG(DBG, "Note OFF: "
                                         << "channel " << event.m.get_channel() << ' ' << "note " << (int)event.m.bytes[1] << ' ' << "velocity " << (int)event.m.bytes[2] << ' ');
                            break;
                        case libremidi::message_type::CONTROL_CHANGE:
                            LOG(DBG, "Control: "
                                         << "channel " << event.m.get_channel() << ' ' << "control " << (int)event.m.bytes[1] << ' ' << "value " << (int)event.m.bytes[2] << ' ');
                            break;
                        case libremidi::message_type::PROGRAM_CHANGE:
                            LOG(DBG, "Program: "
                                         << "channel " << event.m.get_channel() << ' ' << "program " << (int)event.m.bytes[1] << ' ');
                            break;
                        case libremidi::message_type::AFTERTOUCH:
                            LOG(DBG, "Aftertouch: "
                                         << "channel " << event.m.get_channel() << ' ' << "value " << (int)event.m.bytes[1] << ' ');
                            break;
                        case libremidi::message_type::POLY_PRESSURE:
                            LOG(DBG, "Poly pressure: "
                                         << "channel " << event.m.get_channel() << ' ' << "note " << (int)event.m.bytes[1] << ' ' << "value " << (int)event.m.bytes[2] << ' ');
                            break;
                        case libremidi::message_type::PITCH_BEND:
                            LOG(DBG, "Poly pressure: "
                                         << "channel " << event.m.get_channel() << ' ' << "bend " << (int)((event.m.bytes[1] << 7) + event.m.bytes[2]) << ' ');
                            break;
                        case libremidi::message_type::SYSTEM_EXCLUSIVE:
                            LOG(DBG, "SYSTEM_EXCLUSIVE");
                            break;
                        case libremidi::message_type::TIME_CODE:
                            LOG(DBG, "TIME_CODE");
                            break;
                        case libremidi::message_type::SONG_POS_POINTER:
                            LOG(DBG, "SONG_POS_POINTER");
                            break;
                        case libremidi::message_type::SONG_SELECT:
                            LOG(DBG, "SONG_SELECT");
                            break;
                        case libremidi::message_type::RESERVED1:
                            LOG(DBG, "RESERVED1");
                            break;
                        case libremidi::message_type::RESERVED2:
                            LOG(DBG, "RESERVED2");
                            break;
                        case libremidi::message_type::TUNE_REQUEST:
                            LOG(DBG, "TUNE_REQUEST");
                            break;
                        case libremidi::message_type::EOX:
                            LOG(DBG, "EOX");
                            break;
                        case libremidi::message_type::TIME_CLOCK:
                            LOG(DBG, "TIME_CLOCK");
                            break;
                        case libremidi::message_type::RESERVED3:
                            LOG(DBG, "RESERVED3");
                            break;
                        case libremidi::message_type::START:
                            LOG(DBG, "START");
                            break;
                        case libremidi::message_type::CONTINUE:
                            LOG(DBG, "CONTINUE");
                            break;
                        case libremidi::message_type::STOP:
                            LOG(DBG, "STOP");
                            break;
                        case libremidi::message_type::RESERVED4:
                            LOG(DBG, "RESERVED4");
                            break;
                        case libremidi::message_type::ACTIVE_SENSING:
                            LOG(DBG, "ACTIVE_SENSING");
                            break;
                        case libremidi::message_type::SYSTEM_RESET:
                            LOG(DBG, "SYSTEM_RESET");
                            break;
                        default:
                            LOG(DBG, "Unsupported.");
                            break;
                    }
                }
            }
        }
    }
    return true;
}

void midi_probe()
{
    try
    {
        // Create an api map.
        std::map<libremidi::API, std::string> apiMap{
            {libremidi::API::MACOSX_CORE, "OS-X CoreMidi"},
            {libremidi::API::WINDOWS_MM, "Windows MultiMedia"},
            {libremidi::API::UNIX_JACK, "Jack Client"},
            {libremidi::API::LINUX_ALSA_SEQ, "Linux ALSA (sequencer)"},
            {libremidi::API::LINUX_ALSA_RAW, "Linux ALSA (raw)"},
            {libremidi::API::DUMMY, "Dummy (no driver)"},
        };

        auto apis = libremidi::available_apis();

        LOG(DBG, "Compiled APIs:");
        for (auto& api : libremidi::available_apis())
        {
            LOG(DBG, "  " << apiMap[api]);
        }

        {
            libremidi::midi_in midiin;
            LOG(DBG, "Current input API: " << apiMap[midiin.get_current_api()]);

            // Check inputs.
            auto nPorts = midiin.get_port_count();
            LOG(DBG, "There are " << nPorts << " MIDI input sources available.");

            for (unsigned i = 0; i < nPorts; i++)
            {
                std::string portName = midiin.get_port_name(i);
                LOG(DBG, "  Input Port #" << i + 1 << ": " << portName);
            }
        }

        {
            libremidi::midi_out midiout;
            LOG(DBG, "Current output API: " << apiMap[midiout.get_current_api()]);

            // Check outputs.
            auto nPorts = midiout.get_port_count();
            LOG(DBG, "There are " << nPorts << " MIDI output ports available.");

            for (unsigned i = 0; i < nPorts; i++)
            {
                std::string portName = midiout.get_port_name(i);
                LOG(DBG, "  Output Port #" << i + 1 << ": " << portName);
            }
            std::cout << std::endl;
        }
    }
    catch (const libremidi::midi_exception& error)
    {
        UNUSED(error);
        LOG(ERR, error.what());
    }
}

} //namespace Zing

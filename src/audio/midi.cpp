#include <zing/pch.h>

#include <libremidi/libremidi.hpp>

namespace Zing
{

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

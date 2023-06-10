#include "pch.h"

#include "demo.h"

#include <zest/file/file.h>
#include <zest/settings/settings.h>
#include <zest/ui/layout_manager.h>

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

// Playing a note using a custom synth demo
std::atomic<bool> playingNote = false;
std::atomic<bool> playNote = false;
milliseconds noteStartTime = 0ms;
sp_osc* osc = nullptr;
sp_ftbl* ft = nullptr;
sp_phaser* phs = nullptr;

bool showMidiWindow = true;
bool showAudioSettings = true;
bool showAudio = true;
bool showProfiler = true;
bool showDebugSettings = false;
bool showDemoWindow = false;

std::future<void> fontLoaderFuture;
std::future<std::shared_ptr<libremidi::reader>> midiReaderFuture;

} //namespace

// A simple command which uses the wave table to play a note with a phaser.
// Just an example of generating custom audio
void demo_synth_note(float* pOut, uint32_t samples)
{
    auto& ctx = GetAudioContext();

    if (playNote)
    {
        // Create a wavetable and an an oscillator
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

        playNote = false;
        playingNote = true;
        noteStartTime = duration_cast<milliseconds>(timer_get_elapsed(ctx.m_masterClock));
    }
    else
    {
        if (!playingNote)
        {
            return;
        }
    }

    auto currentTime = duration_cast<milliseconds>(timer_get_elapsed(ctx.m_masterClock));
    auto expired = (currentTime - noteStartTime).count() > 1000;

    if (playingNote && expired)
    {
        playingNote = false;
        return;
    }

    for (uint32_t i = 0; i < samples; i++)
    {
        float out[2] = {0.0f, 0.0f};

        sp_osc_compute(ctx.pSP, osc, &out[0], &out[0]);
        sp_phaser_compute(ctx.pSP, phs, &out[0], &out[0], &out[0], &out[1]);

        for (uint32_t ch = 0; ch < ctx.outputState.channelCount; ch++)
        {
            *pOut++ += out[ch];
        }
    }
}

auto demo_load_example_midi()
{
    midiReaderFuture = std::async([]() {
        auto midiFile = file_read(Zest::runtree_find_path("samples/midi/demo-1.mid"));
        auto pReader = std::make_shared<libremidi::reader>();
        auto midiRes = pReader->parse((uint8_t*)midiFile.c_str(), midiFile.size());
        if (midiRes != reader::validated)
        {
            pReader.reset();
        }
        else
        {
            audio_calculate_midi_timings(pReader->tracks, pReader->ticksPerBeat);
        }
        return pReader;
    });
}

void demo_load_gm_sound_font()
{
    fontLoaderFuture = std::async([]() {
        auto& ctx = GetAudioContext();

        // Put a nicer/bigger soundfont here to hear a better rendition.
        //samples_add(ctx.m_samples, "GM", Zest::runtree_find_path("samples/sf2/LiveHQ.sf2"));
        samples_add(ctx.m_samples, "GM", Zest::runtree_find_path("samples/sf2/233_poprockbank.sf2"));
    });
}

void demo_register_windows()
{
    layout_manager_register_window("midi", "Midi", &showMidiWindow);
    layout_manager_register_window("profiler", "Profiler", &showProfiler);
    layout_manager_register_window("audio", "Audio State", &showAudio);
    layout_manager_register_window("settings", "Audio Settings", &showAudioSettings);
    layout_manager_register_window("debug_settings", "Debug Settings", &showDebugSettings);
    layout_manager_register_window("demo_window", "Demo Window", &showDemoWindow);

    layout_manager_load_layouts_file("zing", [](const std::string& name, const LayoutInfo& info) {
        if (!info.windowLayout.empty())
        {
            ImGui::LoadIniSettingsFromMemory(info.windowLayout.c_str());
        }
    });
}

void demo_init()
{
    auto& ctx = GetAudioContext();

    // Lock the ticker to avoid loading conflicts (we unlock when fonts are loaded)
    ctx.audioTickEnableMutex.lock();

    demo_register_windows();

    // Sound font contains a range of instruments, live sampled
    demo_load_gm_sound_font();

    // Midi file example
    demo_load_example_midi();

    demo_draw_midi_init();

    audio_init([=](const std::chrono::microseconds hostTime, void* pOutput, std::size_t numSamples) {
        // Do extra audio synth work here
        demo_synth_note((float*)pOutput, uint32_t(numSamples));
    });
}

// Called outside of the the ImGui frame
void demo_tick()
{
    auto& ctx = GetAudioContext();

    layout_manager_update();

    if (is_future_ready(fontLoaderFuture) && is_future_ready(midiReaderFuture))
    {
        auto pReader = midiReaderFuture.get();
        if (pReader)
        {
            auto& ctx = GetAudioContext();
            auto time = timer_to_ms(timer_get_elapsed(ctx.m_masterClock));
            time += 2000.0;

            // Queue the midi; just for the demo, reset the time to be 2 seconds in the future
            for (const auto& track : pReader->tracks)
            {
                for (const auto& msg : track)
                {
                    auto m = msg.m;
                    m.timestamp = m.timestamp + time;
                    audio_add_midi_event(m);
                }
            }
        }
        ctx.audioTickEnableMutex.unlock();
    }
}

void demo_draw_menu()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("Window"))
        {
            layout_manager_do_menu();
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    layout_manager_do_menu_popups();
}

void demo_draw()
{
    PROFILE_SCOPE(demo_draw)

    auto& ctx = GetAudioContext();
    auto hostTime = duration_cast<milliseconds>(ctx.m_link.clock().micros());

    demo_draw_menu();

    if (showDemoWindow)
    {
        ImGui::ShowDemoWindow(&showDemoWindow);
    }

    // Settings
    if (showDebugSettings)
    {
        Zest::GlobalSettingsManager::Instance().DrawGUI("Settings", &showDebugSettings);
    }

    // Profiler
    if (showProfiler)
    {
        if (ImGui::Begin("Profiler", &showProfiler))
        {
            Zest::Profiler::ShowProfile();
        }
        ImGui::End();
    }

    // Audio and link settings
    if (showAudioSettings)
    {
        if (ImGui::Begin("Audio Settings", &showAudioSettings))
        {
            audio_show_settings_gui();
        }
        ImGui::End();
    }

    // Link state, Audio settings, metronome, etc.
    if (showAudio)
    {
        if (ImGui::Begin("Audio", &showAudio))
        {
            ImGui::SeparatorText("Link");
            audio_show_link_gui();

            ImGui::SeparatorText("Test");
            ImGui::BeginDisabled(ctx.outputState.channelCount == 0 ? true : false);

            if (ImGui::Button("Play Note"))
            {
                playNote = true;
            }

            bool metro = ctx.settings.enableMetronome;
            if (ImGui::Checkbox("Metronome", &metro))
            {
                ctx.settings.enableMetronome = metro;
            }

            bool midi = ctx.settings.enableMidi;
            if (ImGui::Checkbox("Midi Synth Out", &midi))
            {
                ctx.settings.enableMidi = midi;
            }
            ImGui::EndDisabled();

            ImGui::SeparatorText("Analysis");
            demo_draw_analysis();
        }

        ImGui::End();
    }

    // Show the midi timeline
    if (showMidiWindow)
    {
        if (ImGui::Begin("Midi", &showMidiWindow))
        {
            demo_draw_midi();
        }
        ImGui::End();
    }
}

void demo_cleanup()
{
    layout_manager_save();

    // Get the settings
    audio_destroy();
}

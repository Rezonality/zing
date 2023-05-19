#include <filesystem>
#include <fmt/format.h>
#include <memory>

#include <zest/math/imgui_glm.h>
#include <zest/settings/settings.h>
#include <zest/time/profiler.h>
#include <zest/time/timer.h>
#include <zest/file/runtree.h>
#include <zest/file/file.h>

#include <zing/audio/audio.h>

#include <config_zing_app.h>

using namespace Zing;
using namespace std::chrono;
namespace fs = std::filesystem;

namespace {

bool playNote = false;
sp_osc* osc = nullptr;
sp_ftbl* ft = nullptr;
sp_phaser *phs = nullptr;

}

void demo_init()
{
    auto& ctx = GetAudioContext();

    audio_init([=](const std::chrono::microseconds hostTime, void* pOutput, std::size_t numSamples) {
        auto& ctx = GetAudioContext();

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
    });
}

void demo_draw_analysis()
{
    auto& audioContext = GetAudioContext();

    size_t bufferWidth = 512; // default width if no data
    const auto Channels = audioContext.analysisChannels.size();
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

    // Settings
    Zest::GlobalSettingsManager::Instance().DrawGUI("Settings");

    // Profiler
    ImGui::Begin("Profiler");
    static bool open = true;
    Zest::Profiler::ShowProfile(&open);
    ImGui::End();

    // Audio and link
    ImGui::Begin("Audio Settings");
    audio_show_gui();
    ImGui::End();

    auto& ctx = GetAudioContext();
    ImGui::Begin("Synth");
    ImGui::BeginDisabled(ctx.outputState.channelCount == 0 ? true : false);
    ImGui::Checkbox("Beep", &playNote);
    ImGui::EndDisabled();
    ImGui::End();
}

void demo_cleanup()
{
    //demo_save_settings();

    // Get the settings
    audio_destroy();
}

void demo_save_settings()
{
    /*
    toml::table tbl;
    try
    {
        tbl = toml::parse_file(settings_path.string());
        LOG(INFO, "Config:\n" << tbl);

        appConfig.main_window_size = toml_read_vec2<glm::vec2>(tbl["settings"]["main_window_size"]);
        appConfig.main_window_pos = toml_read_vec2<glm::vec2>(tbl["settings"]["main_window_pos"]);
        appConfig.main_window_state = WindowState(tbl["settings"]["main_window_state"].value_or(int(WindowState::Normal)));
        appConfig.last_folder_path = fs::path(tbl["settings"]["last_folder_path"].value_or(""));
        
        appConfig.draw_on_background = tbl["settings"]["draw_on_background"].value_or(false);
        appConfig.transparent_editor = tbl["settings"]["transparent_editor"].value_or(false);

        auto pAnalysisTable = tbl["settings"]["audio_analysis"].as_table();
        auto pDeviceTable = tbl["settings"]["audio_device"].as_table();
        if (pAnalysisTable)
        {
            appConfig.audioAnalysisSettings = audioanalysis_load_settings(*pAnalysisTable);
        }

        if (pDeviceTable)
        {
            appConfig.audioDeviceSettings = audiodevice_load_settings(*pDeviceTable);
        }
    }
    catch (const toml::parse_error& err)
    {
    
    }
    */
}

void demo_load_settings()
{
    /*
    toml::table settings;
    settings.insert_or_assign("project_root", appConfig.project_root.string());

    // Window
    toml_write_vec2(settings, "main_window_size", appConfig.main_window_size);
    toml_write_vec2(settings, "main_window_pos", appConfig.main_window_pos);
    settings.insert_or_assign("main_window_state", int(appConfig.main_window_state));

    settings.insert_or_assign("draw_on_background", appConfig.draw_on_background);
    settings.insert_or_assign("transparent_editor", appConfig.transparent_editor);

    settings.insert_or_assign("last_folder_path", appConfig.last_folder_path.string());

    toml::table analysis_settings = Audio::audioanalysis_save_settings(Audio::GetAudioContext().audioAnalysisSettings);
    toml::table device_settings = Audio::audiodevice_save_settings(Audio::GetAudioContext().audioDeviceSettings);

    settings.insert_or_assign("audio_analysis", analysis_settings);
    settings.insert_or_assign("audio_device", device_settings);
    
    toml::table tbl;
    tbl.insert("settings", settings);

    std::ostringstream str;
    str << tbl;

    Zest::file_write(settings_path, str.str());
    */
}

#pragma once

#include <vector>
#include <zest/file/toml_utils.h>
#include <zest/logger/logger.h>

#undef ERROR

namespace Zing
{

struct AudioDeviceSettings
{
    bool enableInput = true;
    bool enableOutput = false;
    int apiIndex = 0;
    int inputDevice = -1;
    int outputDevice = -1;
    uint32_t inputChannels = 2;
    uint32_t outputChannels = 2;
    uint32_t sampleRate = 0;        // 0 is default / preferred rate
    uint32_t frames = 1024;          // default frames
};

inline toml::table audiodevice_save_settings(const AudioDeviceSettings& settings)
{
    toml::table tab;
    tab.insert_or_assign("api", int(settings.apiIndex));
    tab.insert_or_assign("frames", int(settings.frames));
    tab.insert_or_assign("sample_rate", int(settings.sampleRate));

    tab.insert_or_assign("output_enable", bool(settings.enableOutput));
    tab.insert_or_assign("output_channels", int(settings.outputChannels));
    tab.insert_or_assign("output_device", int(settings.outputDevice));
    
    tab.insert_or_assign("input_enable", bool(settings.enableInput));
    tab.insert_or_assign("input_channels", int(settings.inputChannels));
    tab.insert_or_assign("input_device", int(settings.inputDevice));
    return tab;
}

inline AudioDeviceSettings audiodevice_load_settings(const toml::table& settings)
{
    AudioDeviceSettings deviceSettings;

    if (settings.empty())
        return deviceSettings;

    // TODO: Make string keys to save duplication between load/save functions
    try
    {
        deviceSettings.apiIndex = settings["api"].value_or(int(deviceSettings.apiIndex));
        deviceSettings.frames = settings["frames"].value_or(int(deviceSettings.frames));
        deviceSettings.sampleRate = settings["sample_rate"].value_or(deviceSettings.sampleRate);

        deviceSettings.enableOutput = settings["output_enable"].value_or(deviceSettings.enableOutput);
        deviceSettings.outputChannels = settings["output_channels"].value_or(int(deviceSettings.outputChannels));
        deviceSettings.outputDevice = settings["output_device"].value_or(int(deviceSettings.outputDevice));

        deviceSettings.enableInput = settings["input_enable"].value_or(deviceSettings.enableInput);
        deviceSettings.inputChannels = settings["input_channels"].value_or(int(deviceSettings.inputChannels));
        deviceSettings.inputDevice = settings["input_device"].value_or(int(deviceSettings.inputDevice));
    }
    catch (std::exception & ex)
    {
        LOG(ERROR, ex.what());
    }
    return deviceSettings;
}

} // Audio

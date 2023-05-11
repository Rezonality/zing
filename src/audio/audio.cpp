#include <chrono>
#include <imgui.h>
#include <zing/audio/audio.h>
#include <zing/audio/audio_analysis.h>
//#include <zing/memory.h>

#include <zing/imgui/imgui_utils.h>

using namespace std::chrono;

namespace Zing
{
void audio_validate_rates();
void audio_set_channels_rate(int outputChannels, int inputChannels, uint32_t outputRate, uint32_t inputRate);
void audio_enumerate_devices();
void audio_dump_devices();
void audio_validate_settings();

namespace
{

std::vector<uint32_t> frameSizes{ 128, 256, 512, 1024, 2048, 4096 };
std::vector<std::string> frameNames{ "128", "256", "512", "1024", "2048", "4096" };
std::vector<double> sampleRates = {
    8000.0, 9600.0, 11025.0, 12000.0, 16000.0, 22050.0, 24000.0, 32000.0,
    44100.0, 48000.0, 88200.0, 96000.0, 192000.0
};

uint32_t defaultFrameIndex = 1;
AudioContext audioContext;

} // namespace

AudioContext& GetAudioContext()
{
    return audioContext;
}

std::shared_ptr<AudioBundle> audio_get_bundle()
{
    std::shared_ptr<AudioBundle> bundle;
    if (audioContext.spareBundles.try_dequeue(bundle))
    {
        return bundle;
    }

    return std::make_shared<AudioBundle>();
}

void audio_retire_bundle(std::shared_ptr<AudioBundle>& pBundle)
{
    audioContext.spareBundles.enqueue(pBundle);
}

// This tick() function handles sample computation only.  It will be
// called automatically when the system needs a new buffer of audio
// samples.
int audio_tick(const void* inputBuffer, void* outputBuffer, unsigned long nBufferFrames, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void* dataPointer)
{
    auto& ctx = audioContext;
    // PROFILE_REGION(Audio);
    // PROFILE_NAME_THREAD(Audio);

    if (!ctx.m_audioValid)
    {
        return 0;
    }

    ctx.threadId = std::this_thread::get_id();
    ctx.inputState.frames = nBufferFrames;
    ctx.outputState.frames = nBufferFrames;

    double deltaTime = 1.0f / (double)ctx.outputState.sampleRate;
    const double sampleRate = static_cast<double>(ctx.outputState.sampleRate);
    const auto bufferDuration = duration_cast<microseconds>(duration<double>{ nBufferFrames / sampleRate });
    const auto latency = duration_cast<microseconds>(duration<double>{ timeInfo->outputBufferDacTime });
    // const auto hostTime = ctx.m_hostTimeFilter.sampleTimeToHostTime(device->m_sampleTime);
    ctx.m_sampleTime += nBufferFrames;
    // const auto bufferBeginAtOutput = hostTime + latency;
    auto samples = (float*)outputBuffer;
    // const auto bufferBeginAtOutput = std::chrono::microseconds(long(timeInfo->currentTime * 1000000));

    // Ensure TP has same tempo
    // TimeProvider::Instance().SetTempo(sessionState.tempo(), 4.0);

    if (ctx.m_isPlaying)
    {
        if (outputBuffer)
        {
            float beatAtTime = 0.0f;
            float quantum = 0.0f;
            std::chrono::microseconds hostTime(0);
            memset(outputBuffer, 0, nBufferFrames * ctx.outputState.channelCount * sizeof(float));
            if (ctx.m_fnCallback)
            {
                ctx.m_fnCallback(beatAtTime, quantum, hostTime, outputBuffer, inputBuffer, nBufferFrames);
            }
        }
    }

    if (inputBuffer)
    {
        for (uint32_t i = 0; i < ctx.inputState.channelCount; i++)
        {
            if (ctx.analysisChannels.size() > i)
            {
                // Copy the audio data into a processing bundle and add it to the queue
                auto pBundle = audio_get_bundle();
                pBundle->data.resize(nBufferFrames);

                // Copy with stride
                auto stride = ctx.inputState.channelCount;
                auto pSource = ((const float*)inputBuffer) + i;
                for (uint32_t count = 0; count < nBufferFrames; count++)
                {
                    pBundle->data[count] = *pSource;
                    pSource += stride;
                }

                // Forward the bundle to the processor
                ctx.analysisChannels[i]->processBundles.enqueue(pBundle);
            }
        }
    }

    ctx.inputState.totalFrames += nBufferFrames;
    ctx.outputState.totalFrames += nBufferFrames;

    return 0;
}

void audio_validate_settings()
{
    auto& ctx = audioContext;
    const auto& getAPI = [&]() { return ctx.m_mapApis[ctx.audioDeviceSettings.apiIndex]; };
    const auto& api = getAPI();

    if (ctx.audioDeviceSettings.outputDevice == -1 || (ctx.audioDeviceSettings.outputDevice >= api.NumOutDevices()))
    {
        if (api.NumOutDevices() > 0)
        {
            ctx.audioDeviceSettings.outputDevice = 0;
        }
        else
        {
            ctx.audioDeviceSettings.enableOutput = false;
        }
    }

    if (ctx.audioDeviceSettings.inputDevice == -1 || (ctx.audioDeviceSettings.inputDevice >= api.NumInDevices()))
    {
        if (api.NumInDevices() > 0)
        {
            ctx.audioDeviceSettings.inputDevice = 0;
        }
        else
        {
            ctx.audioDeviceSettings.enableInput = false;
        }
    }

    audio_validate_rates();
}

void audio_dump_devices()
{
    auto& ctx = audioContext;

    LOG(INFO, "Unique Devices: ");
    for (auto& dev : ctx.m_deviceNames)
    {
        LOG(INFO, dev);
    }

    LOG(INFO, "\nApis: ");
    for (auto& api : ctx.m_apiNames)
    {
        LOG(INFO, api);
    }

    // Link
    for (auto& [api, info] : ctx.m_mapApis)
    {
        LOG(INFO, "\nAPI: " << ctx.m_apiNames[api]);
        for (int i = 0; i < info.NumOutDevices(); i++)
        {
            LOG(INFO, "Out Device: " << i << " : " << info.outDeviceNames[i];);

            auto pDeviceInfo = Pa_GetDeviceInfo(info.outDeviceApiIndices[i]);
            LOG(INFO, "  Output Channels: " << pDeviceInfo->maxOutputChannels);

            LOG(INFO, "  Rate: " << pDeviceInfo->defaultSampleRate);
            LOG(INFO, "  Default low output latency  = " << pDeviceInfo->defaultLowOutputLatency);
        }
        for (int i = 0; i < info.NumInDevices(); i++)
        {
            if ((info.NumOutDevices() > i) && info.inDeviceApiIndices[i] == info.outDeviceApiIndices[i])
            {
                // Same device, don't double report
                continue;
            }
            LOG(INFO, "In Device: " << i << " : " << info.inDeviceNames[i];);

            auto pDeviceInfo = Pa_GetDeviceInfo(info.inDeviceApiIndices[i]);
            LOG(INFO, "  Input Channels: " << pDeviceInfo->maxInputChannels);
            LOG(INFO, "  Rate: " << pDeviceInfo->defaultSampleRate);
            LOG(INFO, "  Default low input latency   = " << pDeviceInfo->defaultLowInputLatency);
        }
    }
}

void audio_enumerate_devices()
{
    auto& ctx = audioContext;
    ctx.m_mapApis.clear();
    ctx.m_deviceNames.clear();
    ctx.m_audioValid = false;
    ctx.m_apiNames.clear();
    ctx.m_mapApis.clear();
    ctx.m_currentRateNames.clear();
    ctx.m_currentRates.clear();

    std::map<uint32_t, std::vector<uint32_t>> mapOutDevices;
    std::map<uint32_t, std::vector<uint32_t>> mapInDevices;

    const auto numDevices = Pa_GetDeviceCount();
    if (numDevices < 0)
    {
        LOG(INFO, "Failed to find audio devices");
        return;
    }

    for (uint32_t i = 0; i < (uint32_t)Pa_GetHostApiCount(); i++)
    {
        auto pInfo = Pa_GetHostApiInfo(i);
        ctx.m_mapApis[i] = ApiInfo{};
        ctx.m_apiNames.push_back(pInfo->name);
    }

    ctx.m_deviceNames.clear();
    for (uint32_t deviceApiIndex = 0; deviceApiIndex < uint32_t(numDevices); deviceApiIndex++)
    {
        auto pDeviceInfo = Pa_GetDeviceInfo(deviceApiIndex);
        auto apiName = Pa_GetHostApiInfo(pDeviceInfo->hostApi)->name;

        // Lengthen device names
        std::string strSearch = pDeviceInfo->name;
        auto itr = std::find_if(ctx.m_deviceNames.begin(), ctx.m_deviceNames.end(), [strSearch](std::string& str) {
            if (str.find(strSearch) != std::string::npos || strSearch.find(str) != std::string::npos)
            {
                return true;
            }
            return false;
        });

        // Index into our global list of unique devices
        auto deviceUniqueIndex = 0;
        if (itr == ctx.m_deviceNames.end())
        {
            deviceUniqueIndex = int(ctx.m_deviceNames.size());
            ctx.m_deviceNames.push_back(pDeviceInfo->name);
        }
        else
        {
            deviceUniqueIndex = int(std::distance(ctx.m_deviceNames.begin(), itr));
            if (ctx.m_deviceNames[deviceUniqueIndex].size() < strSearch.size())
            {
                ctx.m_deviceNames[deviceUniqueIndex] = strSearch;
            }
        }

        // This API's devices
        if (pDeviceInfo->maxOutputChannels != 0)
        {
            mapOutDevices[pDeviceInfo->hostApi].push_back(deviceUniqueIndex);
            ctx.m_mapApis[pDeviceInfo->hostApi].outDeviceApiIndices.push_back(deviceApiIndex);
        }

        if (pDeviceInfo->maxInputChannels != 0)
        {
            mapInDevices[pDeviceInfo->hostApi].push_back(deviceUniqueIndex);
            ctx.m_mapApis[pDeviceInfo->hostApi].inDeviceApiIndices.push_back(deviceApiIndex);
        }

        uint32_t currentOutDevice = uint32_t(ctx.m_mapApis[pDeviceInfo->hostApi].outDeviceApiIndices.size() - 1);
        uint32_t currentInDevice = uint32_t(ctx.m_mapApis[pDeviceInfo->hostApi].inDeviceApiIndices.size() - 1);

        auto checkFormat = [&](PaStreamParameters* inParams, PaStreamParameters* outParams) {
            for (int i = 0; i < sampleRates.size(); i++)
            {
                auto err = Pa_IsFormatSupported(inParams->channelCount != 0 ? inParams : nullptr, outParams->channelCount != 0 ? outParams : nullptr, sampleRates[i]);
                if (err == paFormatIsSupported)
                {
                    if (inParams && inParams->channelCount != 0)
                    {
                        ctx.m_mapApis[pDeviceInfo->hostApi].inSampleRates[currentInDevice].push_back(uint32_t(sampleRates[i]));
                    }
                    if (outParams && outParams->channelCount != 0)
                    {
                        ctx.m_mapApis[pDeviceInfo->hostApi].outSampleRates[currentOutDevice].push_back(uint32_t(sampleRates[i]));
                    }
                }
            }
        };

        PaStreamParameters inParams;
        PaStreamParameters outParams;
        inParams.channelCount = pDeviceInfo->maxInputChannels;
        inParams.device = deviceApiIndex;
        inParams.hostApiSpecificStreamInfo = nullptr;
        inParams.sampleFormat = paFloat32;
        inParams.suggestedLatency = pDeviceInfo->defaultLowInputLatency;

        outParams.channelCount = pDeviceInfo->maxOutputChannels;
        outParams.device = deviceApiIndex;
        outParams.hostApiSpecificStreamInfo = nullptr;
        outParams.sampleFormat = paFloat32;
        outParams.suggestedLatency = pDeviceInfo->defaultLowOutputLatency;

        checkFormat(&inParams, &outParams);
    }

    // Cache names for display
    for (auto& [index, api] : ctx.m_mapApis)
    {
        for (auto& outDevice : mapOutDevices[index])
        {
            api.outDeviceNames.push_back(ctx.m_deviceNames[outDevice]);
        }
        for (auto& inDevice : mapInDevices[index])
        {
            api.inDeviceNames.push_back(ctx.m_deviceNames[inDevice]);
        }
    }

    audio_dump_devices();
}

void audio_set_channels_rate(int outputChannels, int inputChannels, uint32_t outputRate, uint32_t inputRate)
{
    auto& ctx = audioContext;
    ctx.outputState.sampleRate = outputRate;
    ctx.outputState.deltaTime = 1.0f / float(outputRate);
    ctx.outputState.channelCount = outputChannels;
    ctx.outputState.totalFrames = 0;

    ctx.inputState.sampleRate = outputRate;
    ctx.inputState.deltaTime = 1.0f / float(outputRate);
    ctx.inputState.channelCount = inputChannels;
    ctx.inputState.totalFrames = 0;

    // Note inputRate is currently always == outputRate
    // I don't know if these can be different from a device point of view; so for now they always match
    assert(outputRate == inputRate);

    // if (ctx.pSP)
    {
        //  sp_destroy(&ctx.pSP);
        //   ctx.pSP = nullptr;
    }

    // sp_create(&ctx.pSP);
    // ctx.pSP->nchan = channels;
    // ctx.pSP->sr = ctx.sampleRate;
}

void audio_destroy()
{
    auto& ctx = audioContext;

    // Stop the analysis
    audio_analysis_destroy_all();

    // Close the stream; this should not thread lock
    if (ctx.m_pStream)
    {
        Pa_StopStream(ctx.m_pStream);
        Pa_CloseStream(ctx.m_pStream);
    }

    Pa_Terminate();
}

bool audio_init(const AudioCB& fnCallback)
{
    auto& ctx = audioContext;

    ctx.m_audioValid = false;
    ctx.m_fnCallback = fnCallback;

    audio_analysis_destroy_all();

    // One duration initialization of the API and devices
    if (!ctx.m_initialized)
    {
        auto err = Pa_Initialize();
        if (err != paNoError)
        {
            LOG(INFO, "Failed to init port audio");
            return false;
        }

        audio_enumerate_devices();

        ctx.m_initialized = true;
    }

    // Close existing stream
    if (ctx.m_pStream)
    {
        Pa_CloseStream(ctx.m_pStream);
        ctx.m_pStream = nullptr;
    }

    const auto& getAPI = [&]() { return ctx.m_mapApis[ctx.audioDeviceSettings.apiIndex]; };

    audio_validate_settings();

    // Set the max region for our audio profile candles to be the max time we think we have to collect the audio data
    // MUtils::Profiler::SetRegionLimit(((duration_cast<nanoseconds>(seconds(1)).count()) * ctx.audioDeviceSettings.frames / ctx.audioDeviceSettings.sampleRate));

    if (!ctx.audioDeviceSettings.enableInput && !ctx.audioDeviceSettings.enableOutput)
    {
        audio_set_channels_rate(ctx.audioDeviceSettings.outputChannels, ctx.audioDeviceSettings.inputChannels, ctx.audioDeviceSettings.sampleRate, ctx.audioDeviceSettings.sampleRate);
        ctx.m_audioValid = true;
        return true;
    }

    if (ctx.audioDeviceSettings.outputDevice >= 0)
    {
        auto outDeviceApiIndex = getAPI().outDeviceApiIndices[ctx.audioDeviceSettings.outputDevice];
        auto outDeviceInfo = Pa_GetDeviceInfo(outDeviceApiIndex);
        ctx.m_outputParams.channelCount = std::max(uint32_t(1), std::min(ctx.audioDeviceSettings.outputChannels, (uint32_t)outDeviceInfo->maxOutputChannels));
        ctx.m_outputParams.device = outDeviceApiIndex;
        ctx.m_outputParams.hostApiSpecificStreamInfo = nullptr;
        ctx.m_outputParams.sampleFormat = paFloat32;
        ctx.m_outputParams.suggestedLatency = outDeviceInfo->defaultLowOutputLatency;
    }

    if (ctx.audioDeviceSettings.inputDevice >= 0)
    {
        auto inDeviceApiIndex = getAPI().inDeviceApiIndices[ctx.audioDeviceSettings.inputDevice];
        auto inDeviceInfo = Pa_GetDeviceInfo(inDeviceApiIndex);
        ctx.m_inputParams.channelCount = std::max(uint32_t(1), std::min(ctx.audioDeviceSettings.inputChannels, (uint32_t)inDeviceInfo->maxInputChannels));
        ctx.m_inputParams.device = inDeviceApiIndex;
        ctx.m_inputParams.hostApiSpecificStreamInfo = nullptr;
        ctx.m_inputParams.sampleFormat = paFloat32;
        ctx.m_inputParams.suggestedLatency = inDeviceInfo->defaultLowInputLatency;
    }

    PaStreamFlags flags = paNoFlag;

    auto ret = Pa_OpenStream(&ctx.m_pStream, ctx.audioDeviceSettings.enableInput ? &ctx.m_inputParams : nullptr, ctx.audioDeviceSettings.enableOutput ? &ctx.m_outputParams : nullptr, ctx.audioDeviceSettings.sampleRate, ctx.audioDeviceSettings.frames, flags, audio_tick, nullptr);
    if (ret != paNoError)
    {
        LOG(ERROR, Pa_GetErrorText(ret));
        return true;
    }

    ret = Pa_StartStream(ctx.m_pStream);
    if (ret != paNoError)
    {
        Pa_CloseStream(ctx.m_pStream);
        ctx.m_pStream = nullptr;
        LOG(ERROR, Pa_GetErrorText(ret));
        return true;
    }

    // The actual rate that got picked
    ctx.audioDeviceSettings.sampleRate = uint32_t(Pa_GetStreamInfo(ctx.m_pStream)->sampleRate);

    audio_set_channels_rate(ctx.m_outputParams.channelCount, ctx.m_inputParams.channelCount, ctx.audioDeviceSettings.sampleRate, ctx.audioDeviceSettings.sampleRate);

    audio_analysis_create_all();

    ctx.m_audioValid = true;
    return true;
}

void audio_validate_rates()
{
    auto& ctx = audioContext;
    if (!ctx.m_changedDeviceCombo)
    {
        return;
    }

    ctx.m_changedDeviceCombo = false;
    ctx.m_currentRateNames.clear();
    ctx.m_currentRates.clear();

    auto& inRates = ctx.m_mapApis[ctx.audioDeviceSettings.apiIndex].inSampleRates[ctx.audioDeviceSettings.inputDevice];
    auto& outRates = ctx.m_mapApis[ctx.audioDeviceSettings.apiIndex].outSampleRates[ctx.audioDeviceSettings.outputDevice];

    if (ctx.audioDeviceSettings.sampleRate == 0)
    {
        ctx.audioDeviceSettings.sampleRate = 44100;
    }

    auto nearestDiff = std::numeric_limits<int>::max();
    uint32_t nearestRateIndex = 0;
    auto addRate = [&](uint32_t rate) {
        ctx.m_currentRates.push_back(rate);
        ctx.m_currentRateNames.push_back(std::to_string(uint32_t(rate)));

        auto rateDiff = std::abs(int(rate) - int(ctx.audioDeviceSettings.sampleRate));
        if (rateDiff < nearestDiff)
        {
            nearestDiff = rateDiff;
            nearestRateIndex = uint32_t(ctx.m_currentRates.size() - 1);
        }
    };

    if (!inRates.empty() && !outRates.empty())
    {
        for (auto& rate1 : inRates)
        {
            for (auto& rate2 : outRates)
            {
                if (rate1 == rate2)
                {
                    addRate(rate1);
                }
            }
        }
    }
    else
    {
        std::for_each(outRates.begin(), outRates.end(), [&](uint32_t rate) { addRate(rate); });
        std::for_each(inRates.begin(), inRates.end(), [&](uint32_t rate) { addRate(rate); });
        if (outRates.empty())
        {
            ctx.m_mapApis[ctx.audioDeviceSettings.apiIndex].outSampleRates.erase(ctx.audioDeviceSettings.outputDevice);
        }
        if (inRates.empty())
        {
            ctx.m_mapApis[ctx.audioDeviceSettings.apiIndex].inSampleRates.erase(ctx.audioDeviceSettings.inputDevice);
        }
    }

    // Assign a default
    if (!ctx.m_currentRates.empty())
    {
        ctx.audioDeviceSettings.sampleRate = ctx.m_currentRates[nearestRateIndex];
    }
}

bool Combo(const char* label, int* current_item, const std::vector<std::string>& items)
{
    return ImGui::Combo(
        label, current_item, [](void* data, int idx, const char** out_text) {
            *out_text = (*(const std::vector<std::string>*)data)[idx].c_str();
            return true;
        },
        (void*)&items, int(items.size()));
}

void audio_show_gui()
{
    auto& ctx = audioContext;

    bool audioResetRequired = false;
    if (Combo("API", &ctx.audioDeviceSettings.apiIndex, ctx.m_apiNames))
    {
        ctx.m_changedDeviceCombo = true;
    }

    if (ImGui::Checkbox("Enable Input", &audioContext.audioDeviceSettings.enableInput))
    {
        ctx.m_changedDeviceCombo = true;
    }

    if (ImGui::Checkbox("Enable Output", &audioContext.audioDeviceSettings.enableOutput))
    {
        ctx.m_changedDeviceCombo = true;
    }

    auto& api = ctx.m_mapApis[audioContext.audioDeviceSettings.apiIndex];
    if (Combo("Output", &audioContext.audioDeviceSettings.outputDevice, api.outDeviceNames))
    {
        ctx.m_changedDeviceCombo = true;
    }
    if (Combo("Input", &audioContext.audioDeviceSettings.inputDevice, api.inDeviceNames))
    {
        ctx.m_changedDeviceCombo = true;
    }

    if (ctx.m_changedDeviceCombo)
    {
        audioResetRequired = true;
    }

    auto getFrameIndex = [&](uint32_t frameSize) {
        int frameIndex = 0;
        for (auto& iFrame : frameSizes)
        {
            if (iFrame == frameSize)
            {
                break;
            }
            frameIndex++;
        }

        if (frameIndex >= frameNames.size())
        {
            frameIndex = defaultFrameIndex;
        }
        return frameIndex;
    };

    auto frameIndex = getFrameIndex(audioContext.audioDeviceSettings.frames);
    if (Combo("Frame Size", &frameIndex, frameNames))
    {
        audioContext.audioDeviceSettings.frames = frameSizes[frameIndex];
        audioResetRequired = true;
    }

    audio_validate_rates();

    int rateIndex = 0;
    for (auto& iRate : ctx.m_currentRates)
    {
        if (iRate == audioContext.audioDeviceSettings.sampleRate)
        {
            break;
        }
        rateIndex++;
    }

    if (rateIndex >= ctx.m_currentRates.size())
    {
        rateIndex = 0;
    }
    if (Combo("Sample Rate", &rateIndex, ctx.m_currentRateNames))
    {
        audioContext.audioDeviceSettings.sampleRate = uint32_t(ctx.m_currentRates[rateIndex]);
        audioResetRequired = true;
    }

    AudioAnalysisSettings& analysisSettings = ctx.audioAnalysisSettings;

    if (ctx.audioDeviceSettings.enableInput)
    {
        int index = 0;
        auto frameIndex = getFrameIndex(analysisSettings.frames);
        if (Combo("Analysis Frames", &frameIndex, frameNames))
        {
            analysisSettings.frames = frameSizes[frameIndex];
            audioResetRequired = true;
        }
        
        auto spectrumBucketsIndex = getFrameIndex(analysisSettings.spectrumBuckets);
        if (Combo("Spectrum Buckets", &spectrumBucketsIndex, frameNames))
        {
            analysisSettings.spectrumBuckets = frameSizes[spectrumBucketsIndex];
            audioResetRequired = true;
        }

        // Note; negative DB
        float dB = -analysisSettings.audioDecibelRange;
        if (ImGui::SliderFloat("Decibel (DbFS)", &dB, -120.0f, -1.0f))
        {
            // No need to reset the device
            analysisSettings.audioDecibelRange = -dB;
        }

        float blend = analysisSettings.blendFactor;
        if (ImGui::SliderFloat("Blend Time (ms)", &blend, 1.0f, 1000.0f))
        {
            // No need to reset the device
            analysisSettings.blendFactor = blend;
        }

        bool logPartitions = analysisSettings.logPartitions;
        if (ImGui::Checkbox("Log Frequency Partitions", &logPartitions))
        {
            // No need to reset the device
            analysisSettings.logPartitions = logPartitions;
        }
        
        ImGui::SliderFloat("Spectrum Log Curve", &analysisSettings.spectrumSharpness, 2.0f, 100.0f);

        bool blendFFT = analysisSettings.blendFFT;
        if (ImGui::Checkbox("Blend FFT", &blendFFT))
        {
            // No need to reset the device
            analysisSettings.blendFFT = blendFFT;
        }

        bool filterFFT = analysisSettings.filterFFT;
        if (ImGui::Checkbox("Smooth FFT", &filterFFT))
        {
            // No need to reset the device
            analysisSettings.filterFFT = filterFFT;
        }

        /*
        bool removeJitter = analysisSettings.removeFFTJitter;
        if (ImGui::Checkbox("Remove Jitter FFT", &removeJitter))
        {
            analysisSettings.removeFFTJitter = removeJitter;
        }
        */

        auto freq = glm::i32vec4(analysisSettings.spectrumFrequencies);
        if (ImGui::DragIntRange4("Spectrum Bands", freq, 1.0f, 0, 48000))
        {
            analysisSettings.spectrumFrequencies = glm::uvec4(freq);
        }

        auto gains = analysisSettings.spectrumGains;
        if (ImGui::SliderFloat4("Band Gains", &gains.x, 0.0f, 1.0f))
        {
            analysisSettings.spectrumGains = gains;
        }
    }

    if (ImGui::Button("Reset"))
    {
        ctx.audioDeviceSettings = AudioDeviceSettings{};
        ctx.audioAnalysisSettings = AudioAnalysisSettings{};
        audioResetRequired = true;
    }

    // Ensure sensible
    audio_analysis_validate_settings(analysisSettings);

    if (audioResetRequired)
    {
        audio_init(nullptr);
    }

    if (!ctx.m_audioValid)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Invalid Audio Configuration!");
    }
}

} // namespace Zing

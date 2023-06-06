#include <zing/pch.h>

#include <zest/settings/settings.h>

#include <zing/audio/audio.h>
#include <zing/audio/audio_analysis.h>
#include <zing/audio/audio_analysis_settings.h>
#include <zing/audio/audio_device_settings.h>
#include <zing/audio/audio_samples.h>
#include <zing/audio/midi.h>

//#define LIBREMIDI_HEADER_ONLY
//#include <libremidi/libremidi.hpp>

using namespace std::chrono;
using namespace ableton;
using namespace Zest;

namespace Zing
{
void audio_validate_rates();
void audio_set_channels_rate(int outputChannels, int inputChannels, uint32_t outputRate, uint32_t inputRate);
void audio_enumerate_devices();
void audio_dump_devices();
void audio_validate_settings();

namespace
{

std::vector<uint32_t> frameSizes{128, 256, 512, 1024, 2048, 4096};
std::vector<std::string> frameNames{"128", "256", "512", "1024", "2048", "4096"};
std::vector<double> sampleRates = {
    8000.0,
    9600.0,
    11025.0,
    12000.0,
    16000.0,
    22050.0,
    24000.0,
    32000.0,
    44100.0,
    48000.0,
    88200.0,
    96000.0,
    192000.0};

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

LinkData audio_pull_link_data()
{
    auto& ctx = audioContext;
    auto linkData = LinkData{};
    if (ctx.m_linkDataGuard.try_lock())
    {
        linkData.requestedTempo = ctx.m_linkData.requestedTempo;
        ctx.m_linkData.requestedTempo = 0;
        linkData.requestStart = ctx.m_linkData.requestStart;
        ctx.m_linkData.requestStart = false;
        linkData.requestStop = ctx.m_linkData.requestStop;
        ctx.m_linkData.requestStop = false;

        ctx.m_lockFreeLinkData.quantum = ctx.m_linkData.quantum;
        ctx.m_lockFreeLinkData.startStopSyncOn = ctx.m_linkData.startStopSyncOn;

        ctx.m_linkDataGuard.unlock();
    }
    linkData.quantum = ctx.m_lockFreeLinkData.quantum;

    return linkData;
}

void audio_play_metronome(const Link::SessionState sessionState, const double quantum, const std::chrono::microseconds beginHostTime, void* pOutput, const std::size_t numSamples)
{
    PROFILE_SCOPE(Metronome);

    auto& ctx = audioContext;
    using namespace std::chrono;

    // Metronome frequencies
    static const double highTone = 1567.98f;
    static const double lowTone = 1108.73f;

    // 100ms click duration
    static const auto clickDuration = duration<double>{0.1};

    // The number of microseconds that elapse between samples
    const auto microsPerSample = 1e6 / ctx.outputState.sampleRate;
    const auto fracSec = (1.0 / (double)ctx.outputState.sampleRate);

    auto pOut = (float*)pOutput;

    for (std::size_t i = 0; i < numSamples; ++i)
    {
        float amplitude = 0.0f;

        // Compute the host time for this sample and the last.
        const auto hostTime = beginHostTime + microseconds(llround(static_cast<double>(i) * microsPerSample));

        // Only make sound for positive beat magnitudes. Negative beat
        // magnitudes are count-in beats.
        auto beat = sessionState.beatAtTime(hostTime, quantum);
        if (int64_t(beat) > ctx.m_lastClickBeat)
        {
            ctx.m_lastClickBeat = int64_t(beat);

            // If we're within the click duration of the last beat, render
            // the click tone into this sample
            if (!ctx.m_clicking)
            {
                ctx.m_clickFrequency = floor(sessionState.phaseAtTime(hostTime, quantum)) == 0 ? highTone : lowTone;
                ctx.m_clickTime = 0.0;
                ctx.m_clicking = true;
            }
        }

        if (ctx.m_clicking && ctx.m_playMetronome)
        {
            // Simple cosine synth
            amplitude = float(cos(2 * glm::pi<double>() * ctx.m_clickTime * ctx.m_clickFrequency) * (1 - sin(5 * glm::pi<double>() * ctx.m_clickTime)));
            ctx.m_clickTime += fracSec;

            if (ctx.m_clickTime > 0.1)
            {
                ctx.m_clicking = false;
            }
        }
        else
        {
            amplitude = 0.0;
        }

        for (uint32_t ch = 0; ch < ctx.outputState.channelCount; ch++)
        {
            *pOut++ = amplitude;
        }
    }
}

void audio_start_playing()
{
    auto& ctx = audioContext;
    LOCK_GUARD(ctx.m_linkDataGuard, LinkDataGuard);
    ctx.m_linkData.requestStart = true;
    ctx.m_link.enable(true);
}

void audio_pre_callback(const std::chrono::microseconds hostTime, void* pOutput, uint32_t frameCount)
{
    auto& ctx = audioContext;
    const auto engineData = audio_pull_link_data();

    auto sessionState = ctx.m_link.captureAudioSessionState();

    if (engineData.requestStart)
    {
        sessionState.setIsPlaying(true, hostTime);
    }

    if (engineData.requestStop)
    {
        sessionState.setIsPlaying(false, hostTime);
    }

    if (!ctx.m_isPlaying && sessionState.isPlaying())
    {
        // Reset the timeline so that beat 0 corresponds to the time when transport starts
        sessionState.requestBeatAtStartPlayingTime(0, engineData.quantum);
        ctx.m_isPlaying = true;
    }
    else if (ctx.m_isPlaying && !sessionState.isPlaying())
    {
        ctx.m_isPlaying = false;
    }

    if (engineData.requestedTempo > 0)
    {
        // Set the newly requested tempo from the beginning of this buffer
        sessionState.setTempo(engineData.requestedTempo, hostTime);
    }

    // Timeline modifications are complete, commit the results
    ctx.m_link.commitAudioSessionState(sessionState);

    if (ctx.m_isPlaying && pOutput)
    {
        // As long as the engine is playing, generate metronome clicks in
        // the buffer at the appropriate beats.
        audio_play_metronome(sessionState, engineData.quantum, hostTime, pOutput, frameCount);
    }
}

void audio_process_midi(void* pOutput, uint32_t frameCount)
{
    auto& ctx = audioContext;

    auto time_ms = (ctx.m_frameCurrentTime.count() / double(milliseconds(1000).count()));

    // Process midi
    static tml_message msg;
    static bool pendingMessage = false;
    static const uint64_t blockSize = 64;

    uint64_t currentBlockSize = 0;
    assert(frameCount % blockSize == 0);

    auto pOut = (float*)pOutput;

    auto emptyTheBlock = [&]() {
        if (currentBlockSize > 0)
        {
            samples_render(ctx.m_samples, pOut, int(currentBlockSize));
            currentBlockSize = 0;
            pOut += (blockSize * ctx.outputState.channelCount);
        }
    };

    auto pContainer = samples_find(ctx.m_samples);
    if (!pContainer || !pContainer->soundFont)
    {
        return;
    }

    auto tsf = pContainer->soundFont;

    for (uint32_t sample = 0; sample < frameCount; sample++)
    {
        if (!pendingMessage)
        {
            if (ctx.midi.try_dequeue(msg))
            {
                pendingMessage = true;
            }
        }

        if (time_ms >= msg.time)
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

        time_ms += 1000.0 / ctx.outputState.sampleRate;
        currentBlockSize++;

        if (currentBlockSize == blockSize)
        {
            emptyTheBlock();
        }
    }

    emptyTheBlock();
}

// This tick() function handles sample computation only.  It will be
// called automatically when the system needs a new buffer of audio
// samples.
int audio_tick(const void* inputBuffer, void* outputBuffer, unsigned long nBufferFrames, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void* dataPointer)
{
    auto& ctx = audioContext;
    PROFILE_REGION(Audio);
    PROFILE_NAME_THREAD(Audio);

    PROFILE_SCOPE(Tick);

    auto fracSec = (nBufferFrames / (double)ctx.outputState.sampleRate);
    static const uint64_t oneSecondNs = uint64_t(duration_cast<nanoseconds>(seconds(1)).count());

    // Set the max region for our audio profile candles to be the max time we think we have to collect the audio data
    Profiler::SetRegionLimit(uint64_t(fracSec * oneSecondNs));

    if (!ctx.m_audioValid)
    {
        assert(!"Eh?");
        return 0;
    }

    if (ctx.m_totalFrames == 0)
    {
        ctx.m_frameInitTime = ctx.m_link.clock().micros();
        ctx.m_frameCurrentTime = ctx.m_frameInitTime;
    }

    ctx.threadId = std::this_thread::get_id();
    ctx.inputState.frames = nBufferFrames;
    ctx.outputState.frames = nBufferFrames;

    double deltaTime = 1.0f / (double)ctx.outputState.sampleRate;
    const double sampleRate = static_cast<double>(ctx.outputState.sampleRate);
    const auto bufferDuration = duration_cast<microseconds>(duration<double>{nBufferFrames / sampleRate});

    // Link
    auto hostTimeAtFrame = ctx.m_hostTimeFilter.sampleTimeToHostTime(double(ctx.m_totalFrames));
    ctx.m_totalFrames += nBufferFrames;
    const auto bufferBeginAtOutput = hostTimeAtFrame + ctx.m_outputLatency.load();
    // Link

    auto samples = (float*)outputBuffer;

    // Ensure TP has same tempo
    // TimeProvider::Instance().SetTempo(sessionState.tempo(), 4.0);

    audio_pre_callback(hostTimeAtFrame, outputBuffer, nBufferFrames);

    audio_process_midi(outputBuffer, nBufferFrames);

    auto sendAnalysis = [&](auto& state, const float* pBuffer, uint32_t frames) {
        for (uint32_t i = 0; i < state.channelCount; i++)
        {
            if (ctx.analysisChannels.size() > i)
            {
                // Copy the audio data into a processing bundle and add it to the queue
                auto pBundle = audio_get_bundle();
                pBundle->data.resize(nBufferFrames);
                pBundle->channel = i;

                // Copy with stride
                auto stride = state.channelCount;
                auto pSource = pBuffer + i;
                for (uint32_t count = 0; count < frames; count++)
                {
                    pBundle->data[count] = *pSource;
                    pSource += stride;
                }

                // Forward the bundle to the processor
                ctx.analysisChannels[i]->processBundles.enqueue(pBundle);
            }
        }
    };

    if (ctx.m_isPlaying)
    {
        if (outputBuffer)
        {
            if (ctx.m_fnCallback)
            {
                ctx.m_fnCallback(bufferBeginAtOutput, outputBuffer, nBufferFrames);
            }
            sendAnalysis(ctx.outputState, (const float*)outputBuffer, nBufferFrames);
        }
    }

    /*
    if (inputBuffer)
    {
        sendAnalysis(ctx.inputState, (const float*)inputBuffer, nBufferFrames);
    }
    */

    ctx.inputState.totalFrames += nBufferFrames;
    ctx.outputState.totalFrames += nBufferFrames;

    ctx.m_frameCurrentTime += bufferDuration;

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
    CHECK_NOT_AUDIO_THREAD;

    ctx.outputState.sampleRate = outputRate;
    ctx.outputState.deltaTime = 1.0f / float(outputRate);
    ctx.outputState.channelCount = outputChannels;
    ctx.outputState.totalFrames = 0;

    ctx.inputState.sampleRate = outputRate;
    ctx.inputState.deltaTime = 1.0f / float(outputRate);
    ctx.inputState.channelCount = inputChannels;
    ctx.inputState.totalFrames = 0;

    if (!ctx.audioDeviceSettings.enableInput)
    {
        ctx.inputState.channelCount = 0;
    }

    if (!ctx.audioDeviceSettings.enableOutput)
    {
        ctx.outputState.channelCount = 0;
    }
    // Note inputRate is currently always == outputRate
    // I don't know if these can be different from a device point of view; so for now they always match
    assert(outputRate == inputRate);

    if (ctx.pSP)
    {
        sp_destroy(&ctx.pSP);
        ctx.pSP = nullptr;
    }

    sp_create(&ctx.pSP);
    ctx.pSP->nchan = ctx.outputState.channelCount;
    ctx.pSP->sr = ctx.outputState.sampleRate;

    samples_update_rate(ctx.m_samples);
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

    if (ctx.pSP)
    {
        sp_destroy(&ctx.pSP);
        ctx.pSP = nullptr;
    }
}

void audio_add_settings_hooks()
{

    SettingsClient client;
    client.pfnLoad = [](const std::string& location, const toml::table& tbl) -> bool {
        auto& ctx = audioContext;

        if (location == "audio.analysis")
        {
            ctx.audioAnalysisSettings = audioanalysis_load_settings(tbl);
            return true;
        }
        else if (location == "audio.device")
        {
            ctx.audioDeviceSettings = audiodevice_load_settings(tbl);
            return true;
        }

        return false;
    };

    client.pfnSave = [](toml::table& tbl) {
        auto& ctx = audioContext;

        auto audio = toml_sub_table(tbl, "audio");
        audio->insert("analysis", audioanalysis_save_settings(ctx.audioAnalysisSettings));
        audio->insert("device", audiodevice_save_settings(ctx.audioDeviceSettings));
    };

    auto& settings = Zest::GlobalSettingsManager::Instance();
    settings.AddClient(client);
}

bool audio_init(const AudioCB& fnCallback)
{
    auto& ctx = audioContext;

    ctx.m_fnCallback = fnCallback;

    ctx.m_link.setTempoCallback([](auto tempo) {
        auto& ctx = audioContext;
        ctx.m_tempo = tempo;
    });

    ctx.m_link.setNumPeersCallback([](auto numPeers) {
        auto& ctx = audioContext;
        ctx.m_numPeers = int(numPeers);
    });

    ctx.m_link.setStartStopCallback([](auto isPlaying) {
        //auto& ctx = audioContext;
        //ctx.m_isPlaying = isPlaying;
    });

    audio_analysis_destroy_all();
    samples_stop(ctx.m_samples);

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

    ctx.m_audioValid = false;

    const auto& getAPI = [&]() { return ctx.m_mapApis[ctx.audioDeviceSettings.apiIndex]; };

    audio_validate_settings();

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
        if (!ctx.audioDeviceSettings.enableOutput)
        {
            ctx.m_outputParams.channelCount = 0;
        }
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
        if (!ctx.audioDeviceSettings.enableInput)
        {
            ctx.m_inputParams.channelCount = 0;
        }
    }

    PaStreamFlags flags = paNoFlag;

    // Link
    ctx.m_outputLatency.store(std::chrono::microseconds(llround(ctx.m_outputParams.suggestedLatency * 1.0e6)));

    auto ret = Pa_OpenStream(&ctx.m_pStream, ctx.audioDeviceSettings.enableInput ? &ctx.m_inputParams : nullptr, ctx.audioDeviceSettings.enableOutput ? &ctx.m_outputParams : nullptr, ctx.audioDeviceSettings.sampleRate, ctx.audioDeviceSettings.frames, flags, audio_tick, nullptr);
    if (ret != paNoError)
    {
        LOG(ERR, Pa_GetErrorText(ret));
        return true;
    }

    ctx.m_audioValid = true;

    ret = Pa_StartStream(ctx.m_pStream);
    if (ret != paNoError)
    {
        Pa_CloseStream(ctx.m_pStream);
        ctx.m_pStream = nullptr;
        LOG(ERR, Pa_GetErrorText(ret));

        ctx.m_audioValid = false;
        return true;
    }

    // The actual rate that got picked
    ctx.audioDeviceSettings.sampleRate = uint32_t(Pa_GetStreamInfo(ctx.m_pStream)->sampleRate);

    audio_set_channels_rate(ctx.m_outputParams.channelCount, ctx.m_inputParams.channelCount, ctx.audioDeviceSettings.sampleRate, ctx.audioDeviceSettings.sampleRate);

    audio_analysis_create_all();

    audio_start_playing();

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
        (void*)&items,
        int(items.size()));
}

void audio_show_link_gui()
{
    using namespace std;
    auto& ctx = audioContext;

    auto session = ctx.m_link.captureAppSessionState();

    const auto time = ctx.m_link.clock().micros();
    const auto beats = session.beatAtTime(time, ctx.m_lockFreeLinkData.quantum);
    const auto phase = session.phaseAtTime(time, ctx.m_lockFreeLinkData.quantum);
    const auto enabled = ctx.m_link.isEnabled() ? "Yes" : "No";
    const auto startStop = ctx.m_lockFreeLinkData.startStopSyncOn ? "Yes" : "No";
    const auto isPlaying = session.isPlaying() ? "[Playing]" : "[Stopped]";
    std::ostringstream str;
    for (int i = 0; i < ceil(ctx.m_lockFreeLinkData.quantum); ++i)
    {
        if (i < phase)
        {
            str << 'X';
        }
        else
        {
            str << 'O';
        }
    }

    if (ImGui::BeginTable("Ableton Link", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Borders | ImGuiTableFlags_PreciseWidths))
    {

        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Value");

        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Enabled");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text(enabled);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Peers");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text(fmt::format("{}", ctx.m_numPeers).c_str());
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Quantum");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text(fmt::format("{}", ctx.m_lockFreeLinkData.quantum).c_str());
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Start/Stop/Sync");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text(startStop);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Playing");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text(isPlaying);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Tempo");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text(fmt::format("{}", int(ctx.m_tempo)).c_str());
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Beats");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text(fmt::format("{:.2f}", beats).c_str());
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Metro");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text(str.str().c_str());
        ImGui::TableNextRow();

        ImGui::EndTable();
    }
}

void audio_show_settings_gui()
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
        audio_init(ctx.m_fnCallback);
    }

    if (!ctx.m_audioValid)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Invalid Audio Configuration!");
    }
}

std::string audio_to_channel_name(uint32_t channel)
{
    if (channel == 0)
    {
        return "L";
    }
    else if (channel == 1)
    {
        return "R";
    }
    else
    {
        return fmt::format("Ch:{}", channel);
    }
}

void audio_add_midi_event(const tml_message& msg)
{
    auto& ctx = audioContext;

    ctx.midi.enqueue(msg);

    for (auto& fnMidi : ctx.midiClients)
    {
        fnMidi(msg);
    }
}

} // namespace Zing

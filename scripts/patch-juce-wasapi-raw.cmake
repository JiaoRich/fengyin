function(replace_exact file before after)
    file(READ "${file}" content)
    string(FIND "${content}" "${after}" already_patched)
    if(NOT already_patched EQUAL -1)
        return()
    endif()
    string(FIND "${content}" "${before}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "JUCE 9.0.2 RAW patch context not found in ${file}")
    endif()
    string(REPLACE "${before}" "${after}" content "${content}")
    file(WRITE "${file}" "${content}")
endfunction()

# The isolated render-only engine must not probe microphone pins during the
# ASIO constructor's dummy-buffer cycle. Leave other JUCE hosts unchanged.
set(asio "${JUCE_SOURCE_DIR}/modules/juce_audio_devices/native/juce_ASIO_windows.cpp")
replace_exact("${JUCE_SOURCE_DIR}/modules/juce_audio_devices/audio_io/juce_AudioIODevice.h"
    "    virtual bool hasControlPanel() const;"
    "    virtual void* getFengYinAsioInterface() { return nullptr; }\n    virtual bool hasControlPanel() const;")
replace_exact("${asio}"
    "    int getCurrentBufferSizeSamples() override"
    "    void* getFengYinAsioInterface() override { return asioObject; }\n\n    int getCurrentBufferSizeSamples() override")
replace_exact("${asio}"
    "        for (int i = 0; i < jmin (2, (int) totalNumInputChans); ++i)"
    "        for (int i = 0; i <\n           #if FENGYIN_ASIO_RENDER_ONLY\n             0;\n           #else\n             jmin (2, (int) totalNumInputChans);\n           #endif\n             ++i)")

# Drivers may reject the constructor's probe while a jack is changing or the
# endpoint is busy. JUCE must not clear null buffers after that failed probe.
replace_exact("${asio}"
    "                outputFormat[i].clear (bufferInfos[outputBufferIndex + i].buffers[0], preferredBufferSize);\n                outputFormat[i].clear (bufferInfos[outputBufferIndex + i].buffers[1], preferredBufferSize);"
    "                if (auto* buffer = bufferInfos[outputBufferIndex + i].buffers[0])\n                    outputFormat[i].clear (buffer, preferredBufferSize);\n                if (auto* buffer = bufferInfos[outputBufferIndex + i].buffers[1])\n                    outputFormat[i].clear (buffer, preferredBufferSize);")

# Keep driver-call time separate from JUCE's subsequent first-callback wait.
# Do not log from the realtime callback itself.
replace_exact("${asio}"
[[            calledback = false;
            err = asioObject->start();]]
[[            calledback = false;
            const auto fengyinStartTime = Time::getMillisecondCounterHiRes();
            err = asioObject->start();
            JUCE_ASIO_LOG ("driver start returned: code=" + String (err)
                + " elapsedMs=" + String (Time::getMillisecondCounterHiRes() - fengyinStartTime, 1)
                + " callbackSeen=" + String (calledback.load() ? 1 : 0));]])
replace_exact("${asio}"
[[                int count = 300;
                while (--count > 0 && ! calledback)
                    Thread::sleep (10);

                isStarted = true;]]
[[                JUCE_ASIO_LOG ("waiting for first driver callback");
                int count = 300;
                while (--count > 0 && ! calledback)
                    Thread::sleep (10);
                JUCE_ASIO_LOG ("first callback wait finished: callbackSeen=" + String (calledback.load() ? 1 : 0));

                isStarted = true;]])

# A reset requested during the constructor probe used to exist only as a
# pending Timer. openDevice()/close() stop that Timer, losing the request.
# Latch it independently, then honour it BEFORE querying channel counts and
# allocating buffers. Keep the change confined to our isolated render engine.
replace_exact("${asio}"
[[    std::atomic<bool> calledback { false };]]
[[    std::atomic<bool> calledback { false };
   #if FENGYIN_ASIO_RENDER_ONLY
    std::atomic<bool> fengyinResetPending { false };
   #endif]])
replace_exact("${asio}"
[[    void resetRequest() noexcept
    {
        startTimer (500);
    }]]
[[    void resetRequest() noexcept
    {
       #if FENGYIN_ASIO_RENDER_ONLY
        fengyinResetPending.store (true);
       #endif
        startTimer (500);
    }]])
replace_exact("${asio}"
[[        isStarted = false;

        auto err = asioObject->getChannels (&totalNumInputChans, &totalNumOutputChans);]]
[[        isStarted = false;

       #if FENGYIN_ASIO_RENDER_ONLY
        if (fengyinResetPending.exchange (false))
        {
            stopTimer();
            JUCE_ASIO_LOG ("honouring pending driver reset before channel discovery");
            if (! removeCurrentDriver())
                return "Driver failed to close for pending reset";
            if (! loadDriver())
                return "Driver failed to reload for pending reset";
            if (const auto initError = initDriver(); initError.isNotEmpty())
                return initError;
            needToReset = false;
            reloadChannelNames();
        }
       #endif

        auto err = asioObject->getChannels (&totalNumInputChans, &totalNumOutputChans);]])

replace_exact("${asio}"
[[        auto err = asioObject->getChannels (&totalNumInputChans, &totalNumOutputChans);
        jassert (err == ASE_OK);]]
[[        auto err = asioObject->getChannels (&totalNumInputChans, &totalNumOutputChans);
        jassert (err == ASE_OK);
       #if FENGYIN_ASIO_RENDER_ONLY
        if (err != ASE_OK || totalNumInputChans < 0 || totalNumOutputChans < 0
            || totalNumInputChans > 4096 || totalNumOutputChans > 4096)
            return "Invalid channel layout after driver initialization";
        // A new jack/driver configuration can have more channels than the
        // constructor probe. Do not reuse arrays sized for the old layout.
        const auto fengyinChannelCapacity = totalNumInputChans + totalNumOutputChans + 4;
        bufferInfos.calloc (fengyinChannelCapacity);
        inputFormat.calloc (fengyinChannelCapacity);
        outputFormat.calloc (fengyinChannelCapacity);
       #endif]])

# Explicit trigger for the same reset path used by driver callbacks. Call after
# private enumeration, before querying channels or allocating stream buffers.
replace_exact("${JUCE_SOURCE_DIR}/modules/juce_audio_devices/audio_io/juce_AudioIODevice.h"
    "    virtual void* getFengYinAsioInterface() { return nullptr; }"
    "    virtual bool requestFengYinAsioReinitialisation() { return false; }\n    virtual void* getFengYinAsioInterface() { return nullptr; }")
replace_exact("${asio}"
    "    void* getFengYinAsioInterface() override { return asioObject; }"
[[    bool requestFengYinAsioReinitialisation() override
    {
       #if FENGYIN_ASIO_RENDER_ONLY
        fengyinResetPending.store (true);
        return true;
       #else
        return false;
       #endif
    }

    void* getFengYinAsioInterface() override { return asioObject; }]])

set(header "${JUCE_SOURCE_DIR}/modules/juce_audio_devices/juce_audio_devices.h")
set(manager "${JUCE_SOURCE_DIR}/modules/juce_audio_devices/audio_io/juce_AudioDeviceManager.cpp")
set(wasapi "${JUCE_SOURCE_DIR}/modules/juce_audio_devices/native/juce_WASAPI_windows.cpp")

replace_exact("${header}"
    "        sharedLowLatency\n    };"
    "        sharedLowLatency,\n        sharedLowLatencyRaw\n    };")

replace_exact("${manager}"
    "    addIfNotNull (list, AudioIODeviceType::createAudioIODeviceType_WASAPI (WASAPIDeviceMode::sharedLowLatency));"
    "    addIfNotNull (list, AudioIODeviceType::createAudioIODeviceType_WASAPI (WASAPIDeviceMode::sharedLowLatency));\n    addIfNotNull (list, AudioIODeviceType::createAudioIODeviceType_WASAPI (WASAPIDeviceMode::sharedLowLatencyRaw));")

replace_exact("${wasapi}"
    "    return deviceMode == WASAPIDeviceMode::sharedLowLatency;"
    "    return deviceMode == WASAPIDeviceMode::sharedLowLatency\n        || deviceMode == WASAPIDeviceMode::sharedLowLatencyRaw;")

replace_exact("${wasapi}"
    "    AUDIO_STREAM_CATEGORY   eCategory;\n};"
    "    AUDIO_STREAM_CATEGORY   eCategory;\n    DWORD                   Options;\n};")

replace_exact("${wasapi}"
[[        if (device != nullptr)
            logFailure (device->Activate (__uuidof (IAudioClient), CLSCTX_INPROC_SERVER,
                                          nullptr, (void**) newClient.resetAndGetPointerAddress()));

        return newClient;]]
[[        if (device != nullptr)
            logFailure (device->Activate (__uuidof (IAudioClient), CLSCTX_INPROC_SERVER,
                                          nullptr, (void**) newClient.resetAndGetPointerAddress()));

        if (newClient != nullptr && deviceMode == WASAPIDeviceMode::sharedLowLatencyRaw)
        {
            auto client2 = newClient.getInterface<IAudioClient2>();
            AudioClientProperties properties{};
            properties.cbSize = sizeof (properties);
            properties.eCategory = AudioCategory_Media;
            properties.Options = 0x1; // AUDCLNT_STREAMOPTIONS_RAW

            if (client2 == nullptr || ! check (client2->SetClientProperties (&properties)))
                newClient = nullptr;
        }

        return newClient;]])

replace_exact("${wasapi}"
    "        if (mode == WASAPIDeviceMode::sharedLowLatency)  return \"Windows Audio (Low Latency Mode)\";"
    "        if (mode == WASAPIDeviceMode::sharedLowLatency)  return \"Windows Audio (Low Latency Mode)\";\n        if (mode == WASAPIDeviceMode::sharedLowLatencyRaw) return \"Windows Audio (RAW Test Mode)\";")

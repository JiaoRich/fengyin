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
replace_exact("${asio}"
    "        for (int i = 0; i < jmin (2, (int) totalNumInputChans); ++i)"
    "        for (int i = 0; i <\n           #if FENGYIN_ASIO_RENDER_ONLY\n             0;\n           #else\n             jmin (2, (int) totalNumInputChans);\n           #endif\n             ++i)")

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

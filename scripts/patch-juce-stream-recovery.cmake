# Narrow JUCE 9.0.2 additions. Reapplied for local and Windows builds.
set(midi_dir "${JUCE_SOURCE_DIR}/modules/juce_audio_devices/midi_io")
replace_exact("${midi_dir}/juce_MidiMessageCollector.h"
    "    void ensureStorageAllocated (size_t bytes);"
    "    void ensureStorageAllocated (size_t bytes);\n    // Set before callbacks start. The caller must own a bounded upstream queue.\n    void setFengYinRetainMessages (bool retain) noexcept { fengyinRetainMessages = retain; }\n    uint64_t getFengYinLateMessages() const noexcept { return fengyinLateMessages.load(); }")
replace_exact("${midi_dir}/juce_MidiMessageCollector.h"
    "    double lastCallbackTime = 0;"
    "    bool fengyinRetainMessages = false;\n    std::atomic<uint64_t> fengyinLateMessages { 0 };\n    double lastCallbackTime = 0;")
replace_exact("${midi_dir}/juce_MidiMessageCollector.cpp"
    "    if (sampleNumber > sampleRate)"
    "    if (! fengyinRetainMessages && sampleNumber > sampleRate)")
replace_exact("${midi_dir}/juce_MidiMessageCollector.cpp"
    "                iter = incomingMessages.findNextSamplePosition (startSample);"
    "                if (! fengyinRetainMessages)\n                    iter = incomingMessages.findNextSamplePosition (startSample);")
replace_exact("${midi_dir}/juce_MidiMessageCollector.cpp"
    "                const auto pos = ((meta.samplePosition - startSample) * scale) >> 10;"
    "                if (fengyinRetainMessages && meta.samplePosition < startSample)\n                    fengyinLateMessages.fetch_add (1, std::memory_order_relaxed);\n                const auto pos = ((int64_t (meta.samplePosition) - startSample) * scale) >> 10;")
replace_exact("${midi_dir}/juce_MidiMessageCollector.cpp"
    "destBuffer.addEvent (meta.data, meta.numBytes, jlimit (0, numSamples - 1, pos));"
    "destBuffer.addEvent (meta.data, meta.numBytes, (int) jlimit (int64_t (0), int64_t (numSamples - 1), pos));")

# Expose the backend's actual connection lifetime, not packet inactivity.
replace_exact("${midi_dir}/juce_MidiDevices.h"
    "    void stop();"
    "    void stop();\n    bool isFengYinConnectionAlive() const;")
replace_exact("${midi_dir}/juce_MidiDevices.cpp"
    "    uint8_t getGroup() const"
    "    bool isFengYinConnectionAlive() const { return connection.isAlive(); }\n\n    uint8_t getGroup() const")
replace_exact("${midi_dir}/juce_MidiDevices.cpp"
    "MidiInput::~MidiInput() = default;"
    "MidiInput::~MidiInput() = default;\nbool MidiInput::isFengYinConnectionAlive() const { return pimpl != nullptr && pimpl->isFengYinConnectionAlive(); }")

# Apply this composed patch once as a transaction. Later replacements refine
# earlier insertions, so their individual strings are not an idempotence guard.
configure_file("${CMAKE_CURRENT_LIST_DIR}/../native/Source/AudioStreamRecovery.h" "${JUCE_SOURCE_DIR}/modules/juce_audio_devices/native/fengyin_AudioStreamRecovery.h" COPYONLY)
file(READ "${wasapi}" fengyin_wasapi_source)
if(NOT fengyin_wasapi_source MATCHES "FENGYIN_STREAM_RECOVERY_REV_1")
if(fengyin_wasapi_source MATCHES "fengyinReopenSameClient")
    message(FATAL_ERROR "Partial stream recovery patch: restore pristine JUCE WASAPI source before applying")
endif()
# Shared WASAPI stream recovery, preserving the original device/graph setup.
replace_exact("${JUCE_SOURCE_DIR}/modules/juce_audio_devices/audio_io/juce_AudioIODevice.h"
[[    virtual String getLastError() = 0;]]
[[    virtual String getLastError() = 0;
    virtual bool isFengYinStreamHealthy() const { return true; }
    virtual String getFengYinStreamDiagnostic() const { return {}; }]])
replace_exact("${wasapi}"
[[namespace juce
{
namespace WasapiClasses]]
[[#include "fengyin_AudioStreamRecovery.h"

namespace juce
{
namespace WasapiClasses]])
replace_exact("${wasapi}"
[[    virtual void deviceSessionBecameActive (IMMDevice*) = 0;]]
[[    virtual void deviceSessionBecameActive (IMMDevice*) = 0;
    virtual void fengyinStreamFailed (HRESULT, int) = 0;
    virtual void fengyinSessionDisconnected (AudioSessionDisconnectReason) = 0;]])
replace_exact("${wasapi}"
[[            if (reason == DisconnectReasonFormatChanged)
                owner.deviceSampleRateChanged();]]
[[            if (enabled.load()) owner.delegate.fengyinSessionDisconnected (reason);]])
replace_exact("${wasapi}"
[[            switch (state)
            {]]
[[            if (! enabled.load()) return S_OK;
            switch (state)
            {]])
replace_exact("${wasapi}"
[[        WASAPIDeviceBase& owner;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SessionEventCallback)]]
[[        std::atomic<bool> enabled { true };
        WASAPIDeviceBase& owner;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SessionEventCallback)]])
replace_exact("${wasapi}"
[[    void closeClient()
    {
        if (client != nullptr)]]
[[    void closeClient()
    {
        if (sessionEventCallback != nullptr) sessionEventCallback->enabled.store (false);
        if (client != nullptr)]])
replace_exact("${wasapi}"
[[        updateFormat (isFloat);

        return true;]]
[[        updateFormat (isFloat);
        fengyinOpenedFormat = *format;
        fengyinOpenedPeriod = bufferSizeSamples;

        return true;]])
replace_exact("${wasapi}"
[[    void closeClient()
    {]]
[[    bool fengyinReopenSameClient()
    {
        closeClient();
        client = createClient();
        if (client == nullptr || ! fengyinOpenedFormat) return false;
        const bool ok = isLowLatencyMode (deviceMode)
            ? initialiseLowLatencyClient (fengyinOpenedPeriod, *fengyinOpenedFormat)
            : initialiseStandardClient (fengyinOpenedPeriod, *fengyinOpenedFormat);
        UINT32 frames = 0;
        if (! ok || FAILED (client->GetBufferSize (&frames)) || frames != actualBufferSize)
            return false; // Never silently change period/format to make recovery succeed.
        createSessionEventCallback();
        return SUCCEEDED (client->SetEventHandle (clientEvent));
    }

    void closeClient()
    {]])
replace_exact("${wasapi}"
[[    ComSmartPtr<IAudioSessionControl> audioSessionControl;]]
[[    std::optional<WAVEFORMATEXTENSIBLE> fengyinOpenedFormat;
    int fengyinOpenedPeriod = 0;
    ComSmartPtr<IAudioSessionControl> audioSessionControl;]])
replace_exact("${wasapi}"
[[    bool start()
    {
        auto samplesToDo = getNumSamplesAvailableToCopy();]]
[[    bool fengyinRecoverOutput()
    {
        renderClient = nullptr;
        if (! fengyinReopenSameClient()) return false;
        if (FAILED (client->GetService (__uuidof (IAudioRenderClient),
                                       (void**) renderClient.resetAndGetPointerAddress()))) return false;
        return start();
    }

    bool start()
    {
        auto samplesToDo = getNumSamplesAvailableToCopy();
        if (samplesToDo < 0) return false;]])
replace_exact("${wasapi}"
[[        if (check (renderClient->GetBuffer ((UINT32) samplesToDo, &outputData)))
            renderClient->ReleaseBuffer ((UINT32) samplesToDo, AUDCLNT_BUFFERFLAGS_SILENT);]]
[[        if (samplesToDo > 0)
        {
            const auto get = renderClient->GetBuffer ((UINT32) samplesToDo, &outputData);
            if (FAILED (get)) { delegate.fengyinStreamFailed (get, 2); return false; }
            const auto release = renderClient->ReleaseBuffer ((UINT32) samplesToDo, AUDCLNT_BUFFERFLAGS_SILENT);
            if (FAILED (release)) { delegate.fengyinStreamFailed (release, 3); return false; }
        }]])
replace_exact("${wasapi}"
[[            if (check (client->GetCurrentPadding (&padding)))
                return (int) actualBufferSize - (int) padding;]]
[[            const auto hr = client->GetCurrentPadding (&padding);
            if (FAILED (hr)) { delegate.fengyinStreamFailed (hr, 1); return -1; }
            if (padding > actualBufferSize) { delegate.fengyinStreamFailed (E_UNEXPECTED, 1); return -1; }
            return (int) actualBufferSize - (int) padding;]])
replace_exact("${wasapi}"
[[            int samplesToDo = jmin (getNumSamplesAvailableToCopy(), bufferSize);]]
[[            int samplesToDo = jmin (getNumSamplesAvailableToCopy(), bufferSize);
            if (samplesToDo < 0) return;]])
replace_exact("${wasapi}"
[[                break;
            }

            if (isExclusiveMode (deviceMode) && WaitForSingleObject (clientEvent, 1000) == WAIT_TIMEOUT)
                break;]]
[[                if (! thread.threadShouldExit()) delegate.fengyinStreamFailed (HRESULT_FROM_WIN32 (ERROR_TIMEOUT), 4);
                return;
            }

            if (isExclusiveMode (deviceMode) && WaitForSingleObject (clientEvent, 1000) == WAIT_TIMEOUT)
            {
                delegate.fengyinStreamFailed (HRESULT_FROM_WIN32 (ERROR_TIMEOUT), 4);
                return;
            }]])
replace_exact("${wasapi}"
[[            if (check (renderClient->GetBuffer ((UINT32) samplesToDo, &outputData)))
            {]]
[[            const auto get = renderClient->GetBuffer ((UINT32) samplesToDo, &outputData);
            if (FAILED (get)) { delegate.fengyinStreamFailed (get, 2); return; }
            {]])
replace_exact("${wasapi}"
[[                renderClient->ReleaseBuffer ((UINT32) samplesToDo, 0);]]
[[                const auto release = renderClient->ReleaseBuffer ((UINT32) samplesToDo, 0);
                if (FAILED (release)) { delegate.fengyinStreamFailed (release, 3); return; }]])
replace_exact("${wasapi}"
[[    int getXRunCount() const noexcept override]]
[[    bool isFengYinStreamHealthy() const override { return fengyinHealth.load() == 0; }
    String getFengYinStreamDiagnostic() const override
    {
        return "wasapiState=" + String (fengyinHealth.load())
            + ",hr=" + String::toHexString ((int) fengyinLastError.load())
            + ",operation=" + String (fengyinLastOperation.load())
            + ",disconnect=" + String (fengyinDisconnect.load())
            + ",attempts=" + String (fengyinAttempts.load())
            + ",recoveries=" + String (fengyinRecoveries.load());
    }
    int getXRunCount() const noexcept override]])
replace_exact("${wasapi}"
[[        close();
        lastError.clear();]]
[[        close();
        lastError.clear();
        fengyinFault.store (0); fengyinDisconnect.store (-1); fengyinHealth.store (0);
        fengyinAttempts.store (0);]])
replace_exact("${wasapi}"
[[        while (! threadShouldExit())
        {
            const auto loadedFlags = flags.load (std::memory_order_acquire);]]
[[        fengyin::AudioStreamRecovery recovery;
        bool previouslyStarted = false;
        const bool recoverOutput = outputDevice != nullptr && inputDevice == nullptr && ! isExclusiveMode (deviceMode);
        while (! threadShouldExit())
        {
            const auto loadedFlags = flags.load (std::memory_order_acquire);
            const bool started = (loadedFlags & flagStarted) != 0;
            if (started && ! previouslyStarted) recovery.reset();
            previouslyStarted = started;
            if (recoverOutput)
            {
                // Only the device worker touches COM clients. The graph, voice,
                // transport and recorder stay attached and retain their state.
                const bool healthy = (loadedFlags & flagOutputIsActive) != 0
                    && (loadedFlags & flagShutdown) == 0 && fengyinFault.load() == 0;
                const auto action = recovery.poll (Time::getMillisecondCounterHiRes(), started, healthy,
                                                  fengyinDisconnect.load() != DisconnectReasonSessionLogoff);
                fengyinAttempts.store (recovery.retryCount());
                if (action != fengyin::AudioStreamRecovery::Action::render)
                {
                    fengyinHealth.store (! started ? 0 : action == fengyin::AudioStreamRecovery::Action::exhausted ? 2 : 1);
                    if (action == fengyin::AudioStreamRecovery::Action::retry && ! threadShouldExit())
                    {
                        flags.fetch_and (~(flagShutdown | flagOutputIsActive));
                        fengyinFault.store (0);
                        // close() joins this worker before changing the endpoint.
                        if (outputDevice->fengyinRecoverOutput()) { ++fengyinRecoveries; fengyinHealth.store (0); }
                        else { fengyinStreamFailed (E_FAIL, 5); }
                    }
                    wait (10);
                    continue;
                }
                fengyinHealth.store (0);
            }]])
replace_exact("${wasapi}"
[[            if (! inputDeviceActive && ! outputDeviceActive)
                continue;]]
[[            if (! inputDeviceActive && ! outputDeviceActive)
            {
                wait (10); // Inactive is not silence: never spin an idle CPU core.
                continue;
            }]])
replace_exact("${wasapi}"
[[    std::atomic<int> flags { 0 };]]
[[    void fengyinStreamFailed (HRESULT hr, int operation) override
    {
        fengyinLastError.store (hr); fengyinLastOperation.store (operation);
        fengyinFault.store (hr); fengyinHealth.store (1);
    }
    void fengyinSessionDisconnected (AudioSessionDisconnectReason reason) override
    {
        fengyinDisconnect.store ((int) reason);
        fengyinStreamFailed (MAKE_HRESULT (1, 0x889, 0x4), 6); // AUDCLNT_E_DEVICE_INVALIDATED
        if (inputDevice != nullptr || isExclusiveMode (deviceMode))
            flags.fetch_or (reason == DisconnectReasonFormatChanged ? flagOutputSampleRateDidChange : flagShutdown);
    }
    std::atomic<HRESULT> fengyinFault { 0 }, fengyinLastError { 0 };
    std::atomic<int> fengyinLastOperation { 0 }, fengyinDisconnect { -1 }, fengyinHealth { 0 };
    std::atomic<int> fengyinAttempts { 0 }, fengyinRecoveries { 0 };
    std::atomic<int> flags { 0 };]])

replace_exact("${wasapi}"
[[    void run() override
    {
        setMMThreadPriority();]]
[[    void run() override
    {
        // Client reconstruction uses COM on this worker, never on the UI.
        struct ScopedWorkerCOM {
            HRESULT result = CoInitializeEx (nullptr, COINIT_MULTITHREADED);
            ~ScopedWorkerCOM() { if (SUCCEEDED (result)) CoUninitialize(); }
        } workerCOM;
        setMMThreadPriority();]])
# A broken driver can signal its event repeatedly without freeing any frames.
# Bound that fault path as well as the ordinary event timeout.
replace_exact("${wasapi}"
[[        int offset = 0;

        while (bufferSize > 0)]]
[[        int offset = 0;
        double fengyinNoSpaceSince = -1;

        while (bufferSize > 0)]])
replace_exact("${wasapi}"
[[            if (samplesToDo == 0)
            {
                // This can ONLY occur in non-exclusive mode]]
[[            if (samplesToDo == 0)
            {
                const auto now = Time::getMillisecondCounterHiRes();
                if (fengyinNoSpaceSince < 0) fengyinNoSpaceSince = now;
                if (now - fengyinNoSpaceSince >= 1000)
                {
                    delegate.fengyinStreamFailed (HRESULT_FROM_WIN32 (ERROR_TIMEOUT), 4);
                    return;
                }
                // This can ONLY occur in non-exclusive mode]])
replace_exact("${wasapi}"
[[            bufferSize -= samplesToDo;
            offset += samplesToDo;]]
[[            fengyinNoSpaceSince = -1;
            bufferSize -= samplesToDo;
            offset += samplesToDo;]])
# GetService interfaces are released on their creating thread. Retain only the
# original UI-created references (one set, bounded) until UI close; replacement
# interfaces are created/released by the worker, including its final exit.
replace_exact("${wasapi}"
[[    bool fengyinReopenSameClient()
    {
        closeClient();]]
[[    bool fengyinReopenSameClient()
    {
        if (! fengyinWorkerClient)
        {
            if (sessionEventCallback != nullptr) sessionEventCallback->enabled.store (false);
            if (client != nullptr) client->Stop();
            if (audioSessionControl != nullptr && sessionEventCallback != nullptr)
                audioSessionControl->UnregisterAudioSessionNotification (sessionEventCallback);
            sessionEventCallback = nullptr;
            fengyinOriginalSession = std::move (audioSessionControl);
            fengyinOriginalClient = std::move (client);
            fengyinWorkerClient = true;
            ResetEvent (clientEvent);
        }
        else closeClient();]])
replace_exact("${wasapi}"
[[    void closeClient()
    {]]
[[    void fengyinReleaseOriginalClient()
    {
        fengyinOriginalSession = nullptr; fengyinOriginalClient = nullptr;
        fengyinWorkerClient = false;
    }

    void closeClient()
    {]])
replace_exact("${wasapi}"
[[    std::optional<WAVEFORMATEXTENSIBLE> fengyinOpenedFormat;]]
[[    ComSmartPtr<IAudioClient> fengyinOriginalClient;
    ComSmartPtr<IAudioSessionControl> fengyinOriginalSession;
    bool fengyinWorkerClient = false;
    std::optional<WAVEFORMATEXTENSIBLE> fengyinOpenedFormat;]])
replace_exact("${wasapi}"
[[    bool fengyinRecoverOutput()
    {
        renderClient = nullptr;]]
[[    bool fengyinRecoverOutput()
    {
        if (! fengyinWorkerRender)
        {
            fengyinOriginalRender = std::move (renderClient);
            fengyinWorkerRender = true;
        }
        else renderClient = nullptr;]])
replace_exact("${wasapi}"
[[    ComSmartPtr<IAudioRenderClient> renderClient;]]
[[    void fengyinFinishWorkerRecovery()
    {
        if (fengyinWorkerRender) { closeClient(); renderClient = nullptr; }
    }
    bool fengyinWorkerRender = false;
    ComSmartPtr<IAudioRenderClient> fengyinOriginalRender;
    ComSmartPtr<IAudioRenderClient> renderClient;]])
replace_exact("${wasapi}"
[[        closeClient();
        renderClient = nullptr;
    }]]
[[        closeClient();
        renderClient = nullptr;
        fengyinOriginalRender = nullptr;
        fengyinReleaseOriginalClient();
        fengyinWorkerRender = false;
    }]])
replace_exact("${wasapi}"
[[                outputDevice->copyBuffers (outs.getArrayOfReadPointers(), numOutputBuffers, bufferSize, inputDevice.get(), *this);
            }
        }
    }]]
[[                outputDevice->copyBuffers (outs.getArrayOfReadPointers(), numOutputBuffers, bufferSize, inputDevice.get(), *this);
            }
        }
        if (recoverOutput) outputDevice->fengyinFinishWorkerRecovery();
    }]])

file(APPEND "${wasapi}" "\n// FENGYIN_STREAM_RECOVERY_REV_1\n")
endif()

# JUCE declares its WASAPI interfaces locally; not every SDK error macro is
# imported. Match JUCE's own HRESULT construction, including cached v1 trees.
replace_exact("${wasapi}"
[[        fengyinStreamFailed (AUDCLNT_E_DEVICE_INVALIDATED, 6);]]
[[        fengyinStreamFailed (MAKE_HRESULT (1, 0x889, 0x4), 6); // AUDCLNT_E_DEVICE_INVALIDATED]])

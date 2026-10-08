#include "Asio4AllOutput.h"
#include "NamedSharedAudioRegion.h"
#include "AsioEndpointSelection.h"

namespace fengyin::audioengine
{
bool Asio4AllOutput::start(AudioEngineCore& engine, std::uint32_t requestedFrames,
                         const std::wstring& preferredEndpoint, std::wstring& error, bool requireMatch)
{
    stop();
    type.reset(juce::AudioIODeviceType::createAudioIODeviceType_ASIO());
    if (! type) { error = L"ASIO backend is unavailable"; return false; }
    type->scanForDevices();
    juce::String selected;
    for (const auto& name : type->getDeviceNames(false))
        if (name.containsIgnoreCase("ASIO4ALL")) { selected = name; break; }
    if (selected.isEmpty())
    {
        error = L"ASIO4ALL is not installed (64-bit driver required)";
        stop();
        return false;
    }
    juce::Logger::writeToLog("ASIO startup stage: create driver and probe buffers");
    device.reset(type->createDevice(selected, {}));
    if (! device) { error = L"Cannot create ASIO4ALL device"; stop(); return false; }
    device->close();
    // Ordinary startup must not enumerate or mutate the driver's private state.
    // Keep the 1.1.3 startup path; only an explicit endpoint switch may use
    // the private API. Physical-channel validation below still rejects cables.
    juce::Logger::writeToLog(requireMatch ? "ASIO startup stage: match physical endpoint"
        : "ASIO startup stage: preserve driver configuration (no private API)");
    const bool selectedEndpoint = requireMatch && !preferredEndpoint.empty()
        && selectAsioEndpoint(device->getFengYinAsioInterface(), preferredEndpoint);
    juce::Logger::writeToLog(selectedEndpoint ? "ASIO endpoint selected by KS identity"
        : requireMatch ? "ASIO endpoint identity unavailable"
                       : "ASIO endpoint matching skipped for ordinary startup");
    if (requireMatch && !selectedEndpoint)
    {
        error=L"Cannot match the requested physical output to an ASIO4ALL pin. Use Windows shared output or select the output in the ASIO4ALL panel.";
        stop(); return false;
    }
    juce::BigInteger outputs;
    outputs.setRange(0, 2, true);
    juce::Logger::writeToLog("ASIO startup stage: open render stream");
    const auto result = device->open({}, outputs, engineSampleRate, static_cast<int>(requestedFrames));
    if (result.isNotEmpty()) { error = std::wstring(result.toWideCharPointer()); stop(); return false; }
    const auto actual = device->getCurrentBufferSizeSamples();
    if (actual != static_cast<int>(requestedFrames))
    {
        error = L"ASIO4ALL did not accept the requested buffer: requested="
            + std::to_wstring(requestedFrames) + L", actual=" + std::to_wstring(actual);
        stop();
        return false;
    }
    const auto channelNames = device->getOutputChannelNames();
    juce::Logger::writeToLog("ASIO4ALL opened channels=" + channelNames.joinIntoString(", "));
    if (!selectedEndpoint && (channelNames.size() < 2
        || channelNames[0].containsIgnoreCase("Not Connected")
        || channelNames[1].containsIgnoreCase("Not Connected")
        || channelNames[0].trim().isEmpty() || channelNames[1].trim().isEmpty()))
    {
        error = L"ASIO4ALL has no connected physical output (Not Connected). The Realtek output could not start; audio callbacks alone do not indicate a working device.";
        stop();
        return false;
    }
    for (int index = 0; !selectedEndpoint && index < juce::jmin(2, channelNames.size()); ++index)
        if (channelNames[index].containsIgnoreCase("fengyin")
            || channelNames[index].containsIgnoreCase("cable")
            || channelNames[index].contains(juce::String::fromUTF8("风吟")))
        {
            error = L"ASIO4ALL output points to a virtual cable/speaker. Select only the physical speaker/headphones in the ASIO4ALL panel.";
            stop();
            return false;
        }
    if (actual <= 0 || actual > maximumFramesPerBlock
        || device->getCurrentSampleRate() != engineSampleRate
        || device->getActiveOutputChannels().countNumberOfSetBits() != 2)
    {
        error = L"ASIO4ALL must provide 48 kHz stereo with a buffer no larger than 512 frames";
        stop();
        return false;
    }
    request = CreateSemaphoreW(nullptr, 0, 4, instrumentRequestSemaphoreName);
    if (! request) { error = L"Cannot create instrument request semaphore"; stop(); return false; }
    // An old stopped producer may have left wake-ups. Do not pre-render an
    // arbitrary backlog when the new graph attaches.
    while (WaitForSingleObject(request, 0) == WAIT_OBJECT_0) {}
    core = &engine;
    frames.store(static_cast<std::uint32_t>(actual));
    latency.store(static_cast<std::uint32_t>(juce::jmax(0, device->getOutputLatencyInSamples())));
    lastCallback.store(0);
    device->start(this);
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + 1500.0;
    while (lastCallback.load() == 0 && juce::Time::getMillisecondCounterHiRes() < deadline)
        juce::Thread::sleep(5);
    if (! isRunning())
    {
        error = L"ASIO4ALL opened but produced no audio callbacks; check enabled physical outputs in ASIO4ALL";
        stop();
        return false;
    }
    juce::Logger::writeToLog("Backend=ASIO4ALL driver=" + selected
        + " requested=" + juce::String(requestedFrames) + " actual=" + juce::String(actual)
        + " outputLatency=" + juce::String(latency.load())
        + " outputs=" + device->getOutputChannelNames().joinIntoString(", "));
    return true;
}

void Asio4AllOutput::stop() noexcept
{
    if (device) { device->stop(); device->close(); }
    device.reset();
    type.reset();
    core = nullptr;
    if (request) CloseHandle(request);
    request = nullptr;
    running.store(false);
    frames.store(0);
}

bool Asio4AllOutput::isRunning() const noexcept
{
    const auto last = lastCallback.load();
    return running.load() && last != 0
        && juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks() - last) < 1.0;
}

void Asio4AllOutput::audioDeviceAboutToStart(juce::AudioIODevice* opened)
{
    // A driver reset must never silently change the block size beneath the
    // producer. The engine supervisor will reopen the complete stream.
    running.store(opened->getCurrentBufferSizeSamples() == static_cast<int>(frames.load())
                  && opened->getCurrentSampleRate() == engineSampleRate);
}

void Asio4AllOutput::audioDeviceIOCallbackWithContext(const float* const*, int,
    float* const* outputs, int channels, int samples, const juce::AudioIODeviceCallbackContext&)
{
    lastCallback.store(juce::Time::getHighResolutionTicks());
    if (! running.load() || ! core || samples != static_cast<int>(frames.load()))
    {
        for (int channel = 0; channel < channels; ++channel)
            if (outputs[channel]) juce::FloatVectorOperations::clear(outputs[channel], samples);
        running.store(false);
        return;
    }
    core->render(outputs, static_cast<std::uint32_t>(channels), static_cast<std::uint32_t>(samples));
    ReleaseSemaphore(request, 1, nullptr);
}
}

#include "TestSynthEngine.h"
#include "RecordingService.h"
#include "AccompanimentAudioService.h"
#include "MasterOutputService.h"

#include <algorithm>
#include <cmath>

namespace fengyin
{
void TestSynthEngine::noteOn(int noteNumber, float velocity) noexcept
{
    push({ CommandType::noteOn, noteNumber, std::clamp(velocity, 0.0f, 1.0f) });
}

void TestSynthEngine::noteOff(int noteNumber) noexcept
{
    push({ CommandType::noteOff, noteNumber, 0.0f });
}

void TestSynthEngine::breathChanged(float value) noexcept
{
    push({ CommandType::breath, 0, std::clamp(value, 0.0f, 1.0f) });
}

void TestSynthEngine::pitchBendChanged(float bipolarValue) noexcept
{
    push({ CommandType::pitchBend, 0, std::clamp(bipolarValue, -1.0f, 1.0f) });
}

void TestSynthEngine::resetPerformance() noexcept
{
    for (int note = 0; note < 128; ++note)
        push({ CommandType::noteOff, note, 0.0f });
    push({ CommandType::breath, 0, 0.0f });
    push({ CommandType::pitchBend, 0, 0.0f });
}

void TestSynthEngine::audioDeviceIOCallbackWithContext(const float* const*,
                                                       int,
                                                       float* const* outputs,
                                                       int numOutputs,
                                                       int numSamples,
                                                       const juce::AudioIODeviceCallbackContext&)
{
    applyPendingCommands();
    float peak = 0.0f;
    constexpr auto twoPi = juce::MathConstants<double>::twoPi;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        smoothedBreath += (targetBreath - smoothedBreath) * 0.0025f;
        const auto amplitude = activeNote >= 0 ? smoothedBreath * noteVelocity * 0.18f : 0.0f;
        const auto value = static_cast<float>(std::sin(phase)) * amplitude;
        phase += phaseDelta;
        if (phase >= twoPi)
            phase -= twoPi;
        peak = std::max(peak, std::abs(value));
        for (int channel = 0; channel < numOutputs; ++channel)
            if (outputs[channel] != nullptr)
                outputs[channel][sample] = value;
    }

    if (masterOutput != nullptr)
        masterOutput->processInstrument(outputs, numOutputs, numSamples);
    if (accompaniment != nullptr)
        accompaniment->mixInto(outputs, numOutputs, numSamples);
    if (masterOutput != nullptr)
        masterOutput->processMaster(outputs, numOutputs, numSamples);
    leftPeak.store(peak, std::memory_order_relaxed);
    rightPeak.store(peak * 0.96f, std::memory_order_relaxed);
    if (recorder != nullptr)
        recorder->push(outputs, numOutputs, numSamples);
}

void TestSynthEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    sampleRate = device != nullptr ? device->getCurrentSampleRate() : 48000.0;
    if (accompaniment != nullptr)
        accompaniment->prepare(sampleRate, device != nullptr ? device->getCurrentBufferSizeSamples() : 512);
    if (masterOutput != nullptr)
        masterOutput->setSampleRate(sampleRate);
    phase = 0.0;
    updateFrequency();
}

void TestSynthEngine::audioDeviceStopped()
{
    if (accompaniment != nullptr)
        accompaniment->release();
    leftPeak.store(0.0f, std::memory_order_relaxed);
    rightPeak.store(0.0f, std::memory_order_relaxed);
}

bool TestSynthEngine::push(Command command) noexcept
{
    const auto write = writeIndex.load(std::memory_order_relaxed);
    const auto next = (write + 1) % queueSize;
    if (next == readIndex.load(std::memory_order_acquire))
        return false;
    queue[write] = command;
    writeIndex.store(next, std::memory_order_release);
    return true;
}

bool TestSynthEngine::pop(Command& command) noexcept
{
    const auto read = readIndex.load(std::memory_order_relaxed);
    if (read == writeIndex.load(std::memory_order_acquire))
        return false;
    command = queue[read];
    readIndex.store((read + 1) % queueSize, std::memory_order_release);
    return true;
}

void TestSynthEngine::applyPendingCommands() noexcept
{
    Command command;
    while (pop(command))
    {
        switch (command.type)
        {
            case CommandType::noteOn:
                activeNote = command.note;
                noteVelocity = command.value;
                updateFrequency();
                break;
            case CommandType::noteOff:
                if (activeNote == command.note)
                    activeNote = -1;
                break;
            case CommandType::breath:
                targetBreath = command.value;
                break;
            case CommandType::pitchBend:
                pitchBend = command.value;
                updateFrequency();
                break;
        }
    }
}

void TestSynthEngine::updateFrequency() noexcept
{
    if (activeNote < 0 || sampleRate <= 0.0)
    {
        phaseDelta = 0.0;
        return;
    }
    const auto bentNote = static_cast<double>(activeNote) + static_cast<double>(pitchBend) * 2.0;
    const auto frequency = 440.0 * std::pow(2.0, (bentNote - 69.0) / 12.0);
    phaseDelta = juce::MathConstants<double>::twoPi * frequency / sampleRate;
}
}

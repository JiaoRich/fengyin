#include "Sf2Processor.h"
#include "Sf2ProbeSession.h"

namespace fengyin::sf2
{
Processor::Processor() : AudioProcessor(BusesProperties().withOutput("Stereo", juce::AudioChannelSet::stereo(), true)), Thread("SF2 loader")
{
    addParameter(gain = new juce::AudioParameterFloat({"sf2-gain",1}, juce::String::fromUTF8("方案音量"), 0.f, 2.f, 1.f));
    addParameter(bend = new juce::AudioParameterInt({"sf2-bend",1}, "Pitch Bend Range", 0, 24, 2));
    status = juce::String::fromUTF8("请选择 SF2 文件");
    startThread(juce::Thread::Priority::low);
}
Processor::~Processor()
{
    signalThreadShouldExit(); generation.fetch_add(1); notify();
    stopThread(-1); // child inspection observes cancellation every 20ms
    delete pending.exchange(nullptr); delete retired.exchange(nullptr); delete active.exchange(nullptr);
}
void Processor::prepareToPlay(double sampleRate, int)
{
    outputGain.reset(sampleRate, .02); outputGain.setCurrentAndTargetValue(gain->get());
    resetRequested.store(true);
    const auto before = rate.exchange(sampleRate);
    if (std::abs(before - sampleRate) > .01) {
        auto state = snapshot();
        if (state.hasResource()) request({resourceFile(state.hash), state});
    }
}
void Processor::releaseResources() { resetRequested.store(true); }

void Processor::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    audio.clear();
    if (resetRequested.exchange(false)) {
        notes.reset(); expression = 0;
        if (auto* p = active.load()) p->voice->reset(bend->get());
    }
    // Commit only between phrases. A stale candidate is retired without ever
    // becoming audible. At most one pointer waits for non-realtime deletion.
    if (!retired.load()) {
        auto* candidate = pending.load();
        if (candidate && (candidate->generation != generation.load() || (notes.none() && expression == 0))) {
            candidate = pending.exchange(nullptr);
            if (candidate) {
                if (candidate->generation != generation.load()) retired.store(candidate);
                else retired.store(active.exchange(candidate));
            }
        }
    }
    auto* p = active.load();
    const bool renderable = p && std::abs(p->rate - rate.load()) < .01;
    if (renderable) p->voice->setBendRange(bend->get());
    int offset = 0;
    const auto renderTo = [&](int end) {
        end = juce::jlimit(offset, audio.getNumSamples(), end);
        if (renderable && end > offset && audio.getNumChannels() >= 2)
            p->voice->render(audio.getWritePointer(0, offset), audio.getWritePointer(1, offset), end - offset);
        offset = end;
    };
    for (const auto event : midi) {
        renderTo(event.samplePosition);
        const auto message = event.getMessage();
        if (message.getChannel() != 1) continue;
        if (message.isNoteOn()) notes.set(size_t(message.getNoteNumber()));
        else if (message.isNoteOff()) notes.reset(size_t(message.getNoteNumber()));
        else if (message.isController()) {
            const auto cc = message.getControllerNumber();
            if (cc == 11) expression = message.getControllerValue();
            if (cc == 120 || cc == 123 || cc == 121) { notes.reset(); expression = 0; }
        }
        if (renderable) p->voice->midi(message.getRawData(), message.getRawDataSize());
    }
    renderTo(audio.getNumSamples()); midi.clear();
    outputGain.setTargetValue(gain->get());
    for (int i = 0; i < audio.getNumSamples(); ++i) {
        const auto scale = outputGain.getNextValue();
        for (int ch = 0; ch < audio.getNumChannels(); ++ch) {
            const auto value = audio.getSample(ch, i) * scale;
            audio.setSample(ch, i, std::isfinite(value) ? value : 0.f);
        }
    }
}

State Processor::snapshot() const
{
    const juce::ScopedLock lock(metadataLock); // excludes loader retirement, not rendering
    auto* candidate = pending.load(); auto* current = active.load();
    if(candidate && candidate->generation!=generation.load())candidate=nullptr;
    State state = candidate ? candidate->state : current ? current->state : selection;
    state.prepared = !loading && !awaitingPreset && (candidate || current || state.hash.isEmpty());
    state.ready = !loading && !awaitingPreset && !candidate && current;
    state.gain = gain->get(); state.bend = bend->get(); state.error = lastError;
    return state;
}
State Processor::editorSelection() const
{
    const juce::ScopedLock lock(metadataLock);
    return awaitingPreset ? selection : snapshot();
}
void Processor::getStateInformation(juce::MemoryBlock& data) { data = snapshot().encode(); }
void Processor::setStateInformation(const void* data, int size)
{
    const auto state = State::decode(data, size);
    if (!state) { const juce::ScopedLock lock(metadataLock); lastError = juce::String::fromUTF8("SF2 方案状态无效"); return; }
    *gain = state->gain; *bend = state->bend;
    if (state->hasResource()) request({resourceFile(state->hash), *state});
    else if (state->hash.isEmpty()) { const juce::ScopedLock lock(metadataLock); selection = *state; }
    else { const juce::ScopedLock lock(metadataLock); lastError = juce::String::fromUTF8("SF2 方案未选择预设"); }
}
juce::var Processor::availablePresets() const { const juce::ScopedLock lock(metadataLock); return presets; }
juce::String Processor::statusText() const
{
    const juce::ScopedLock lock(metadataLock);
    if (pending.load()) return juce::String::fromUTF8("已准备，停吹后切换");
    if (!loading && !awaitingPreset && lastError.isEmpty()) if (auto* p = active.load()) return p->state.name;
    return status;
}
void Processor::chooseFile(const juce::File& file) { State state; state.name = file.getFileName(); request({file, state}); }
void Processor::choosePreset(int bank, int program)
{
    State state; { const juce::ScopedLock lock(metadataLock); state = selection; }
    state.bank = bank; state.program = program;
    if (validHash(state.hash)) request({resourceFile(state.hash), state});
}
void Processor::cancelLoad()
{
    generation.fetch_add(1);
    const juce::ScopedLock lock(metadataLock);
    requested = false; loading = false; awaitingPreset=false;
    if(auto* p=active.load()) {selection=p->state;presets=p->presets;} else {selection={};presets=juce::var();}
    lastError.clear(); status = juce::String::fromUTF8("已取消，原音色保留"); notify();
}
void Processor::request(Request request)
{
    request.generation = generation.fetch_add(1) + 1; request.rate = rate.load();
    const juce::ScopedLock lock(metadataLock);
    next = std::move(request); requested = true; loading = true; awaitingPreset=false; lastError.clear();
    status = juce::String::fromUTF8("正在检查并准备音源…"); notify();
}
void Processor::reclaimRetired()
{
    Candidate* previous;
    { const juce::ScopedLock lock(metadataLock); previous=retired.exchange(nullptr); }
    delete previous;
}
void Processor::run()
{
    while (!threadShouldExit()) {
        reclaimRetired();
        Request r;
        { const juce::ScopedLock lock(metadataLock); if (requested) { r = next; requested = false; } }
        if (!r.generation) { wait(25); continue; }
        const auto cancelled = [&] { return threadShouldExit() || r.generation != generation.load(); };
        try {
            // Only the callback claims a published candidate. Do not free a
            // pointer it may currently inspect. It retires stale generations;
            // when audio is stopped this bounded slot waits for audio restart.
            while (pending.load() && !cancelled()) { reclaimRetired(); wait(10); }
            if (cancelled()) continue;
            auto checked = validateFile(r.file);
            if (checked.failed()) throw std::runtime_error(checked.getErrorMessage().toStdString());
            const auto report = inspectInChild(r.file, cancelled);
            if (cancelled()) continue;
            if (!report.isObject() || !bool(report["success"])) throw std::runtime_error(
                report["error"].toString().isNotEmpty() ? report["error"].toString().toStdString() : "SF2 检查未完成，请检查音源组件或文件");
            if (validHash(r.state.hash) && r.state.hash != report["sha256"].toString()) throw std::runtime_error("SF2 音源校验失败");
            auto* list = report["presets"].getArray();
            if (!list || list->isEmpty()) throw std::runtime_error("SF2 没有可用音色");
            juce::String hash;
            checked = importResource(r.file, hash);
            if (checked.failed()) throw std::runtime_error(checked.getErrorMessage().toStdString());
            if (hash != report["sha256"].toString()) throw std::runtime_error("SF2 文件在检查期间发生变化，请重试");
            r.state.hash = hash; r.state.bytes = r.file.getSize();
            if (r.state.bank < 0 && list->size() == 1) { r.state.bank = int((*list)[0]["bank"]); r.state.program = int((*list)[0]["program"]); }
            { const juce::ScopedLock lock(metadataLock); if (cancelled()) continue; presets = report["presets"]; selection = r.state; }
            if (r.state.bank < 0) {
                const juce::ScopedLock lock(metadataLock); if (!cancelled()) { loading = false; awaitingPreset=true; status = juce::String::fromUTF8("文件含多个音色，请选择一个预设"); } continue;
            }
           #if JUCE_WINDOWS
            MEMORYSTATUSEX memory{}; memory.dwLength=sizeof(memory);
            if(!GlobalMemoryStatusEx(&memory) || memory.ullAvailPhys < uint64_t(r.state.bytes)*3+256ull*1024*1024)
                throw std::runtime_error("可用内存不足，原音色保留");
           #endif
            if(cancelled())continue;
            auto candidate = std::make_unique<Candidate>();
            candidate->voice = std::make_unique<Voice>(r.rate);
            candidate->voice->load(resourceFile(hash).getFullPathName().toStdString());
            if (!candidate->voice->select(r.state.bank, r.state.program, r.state.bend)) throw std::runtime_error("SF2 预设已不存在");
            candidate->state = r.state; candidate->state.error.clear(); candidate->presets=report["presets"]; candidate->generation = r.generation; candidate->rate = r.rate;
            const juce::ScopedLock lock(metadataLock);
            if (cancelled()) continue;
            pending.store(candidate.release()); loading = false;
        } catch (const std::exception& e) {
            const juce::ScopedLock lock(metadataLock);
            if (!cancelled()) { loading = false; lastError = juce::String::fromUTF8(e.what()); status = lastError; }
        }
    }
    reclaimRetired();
}
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new fengyin::sf2::Processor(); }

#pragma once
#include <fluidsynth.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace fengyin::sf2
{
struct Preset { int bank = 0, program = 0; std::string name; };

// One synth, no audio/MIDI driver, sequencer, thread or built-in effects. Its
// owner serialises configuration and rendering; all allocation happens before
// the instance is handed to the audio callback.
class Voice final
{
public:
    explicit Voice(double sampleRate)
    {
        if (!std::isfinite(sampleRate) || sampleRate < 8000 || sampleRate > 192000)
            throw std::runtime_error("Unsupported sample rate");
        settings.reset(new_fluid_settings());
        if (!settings) throw std::bad_alloc();
        const auto setting=[](int result){if(result!=FLUID_OK)throw std::runtime_error("SF2 engine setting rejected");};
        setting(fluid_settings_setnum(settings.get(), "synth.sample-rate", sampleRate));
        setting(fluid_settings_setnum(settings.get(), "synth.gain", 0.2));
        setting(fluid_settings_setint(settings.get(), "synth.polyphony", 128));
        setting(fluid_settings_setint(settings.get(), "synth.midi-channels", 16));
        setting(fluid_settings_setint(settings.get(), "synth.cpu-cores", 1));
        setting(fluid_settings_setint(settings.get(), "synth.threadsafe-api", 0));
        setting(fluid_settings_setint(settings.get(), "synth.reverb.active", 0));
        setting(fluid_settings_setint(settings.get(), "synth.chorus.active", 0));
        setting(fluid_settings_setint(settings.get(), "synth.dynamic-sample-loading", 0));
        synth.reset(new_fluid_synth(settings.get()));
        if (!synth) throw std::bad_alloc();
        // Wind instruments sustain a phrase with CC11, not the velocity at
        // the instant a key is pressed. Remove only the engine's two default
        // amplitude modulators; SoundFont-authored modulators remain intact.
        // This API allocates: configure it here, never in midi()/render().
        disableDefaultAttenuation(FLUID_MOD_VELOCITY, FLUID_MOD_GC);
        disableDefaultAttenuation(11, FLUID_MOD_CC);
        rampSamples = std::max(1, int(std::round(sampleRate * .005)));
    }
    std::vector<Preset> load(const std::string& utf8Path)
    {
        soundfont = fluid_synth_sfload(synth.get(), utf8Path.c_str(), 0);
        if (soundfont < 0) throw std::runtime_error("Cannot load SoundFont");
        auto* sf = fluid_synth_get_sfont_by_id(synth.get(), soundfont);
        if (!sf) throw std::runtime_error("Missing SoundFont");
        std::vector<Preset> result;
        fluid_sfont_iteration_start(sf);
        while (auto* preset = fluid_sfont_iteration_next(sf))
        {
            if (result.size() >= 4096) throw std::runtime_error("Too many presets");
            result.push_back({fluid_preset_get_banknum(preset), fluid_preset_get_num(preset), fluid_preset_get_name(preset)});
        }
        if (result.empty()) throw std::runtime_error("SoundFont has no preset");
        return result;
    }
    bool select(int bank, int program, int bend = 2)
    {
        if (bank < 0 || bank > 16383 || program < 0 || program > 127 || bend < 0 || bend > 24) return false;
        if (fluid_synth_program_select(synth.get(), 0, soundfont, bank, program) != FLUID_OK) return false;
        bendRange = bend; reset(bend);
        return true;
    }
    void reset(int bend = 2) noexcept
    {
        bendRange = bend;
        fluid_synth_all_sounds_off(synth.get(), 0);
        fluid_synth_cc(synth.get(), 0, 121, 0);
        fluid_synth_cc(synth.get(), 0, 64, 0);
        fluid_synth_cc(synth.get(), 0, 7, 127);
        fluid_synth_cc(synth.get(), 0, 11, 0);
        breath = targetBreath = breathStep = 0.f; breathRemaining = 0;
        fluid_synth_pitch_wheel_sens(synth.get(), 0, bend);
        fluid_synth_pitch_bend(synth.get(), 0, 8192);
    }
    void midi(const unsigned char* bytes, int size) noexcept
    {
        if (size < 1 || (bytes[0] & 15) != 0) return; // first channel only
        const int kind = bytes[0] & 0xf0;
        const int a = size > 1 ? bytes[1] & 127 : 0, b = size > 2 ? bytes[2] & 127 : 0;
        if (kind == 0x90 && size == 3) {
            if (b) {
                // Stable medium-strong sample layer. Tiny first-note velocity
                // must not lock an otherwise growing breath near silence.
                fluid_synth_noteon(synth.get(), 0, a, 100);
                softenNewNote(a);
            }
            else fluid_synth_noteoff(synth.get(), 0, a);
        } else if (kind == 0x80 && size == 3) fluid_synth_noteoff(synth.get(), 0, a);
        else if (kind == 0xe0 && size == 3) fluid_synth_pitch_bend(synth.get(), 0, a | (b << 7));
        else if (kind == 0xb0 && size == 3) {
            // SF2 capability v1: expression and release only. Program/bank,
            // CC7, mod-wheel and unknown hardware CC cannot rewrite a preset.
            if (a == 11) {
                const auto target = float(b) / 127.f;
                if (target != targetBreath) {
                    targetBreath = target; breathRemaining = rampSamples;
                    breathStep = (targetBreath - breath) / float(rampSamples);
                }
                // Preserve custom SF2 timbre mappings, while our single
                // continuous amplitude path replaces the default attenuation.
                fluid_synth_cc(synth.get(), 0, a, b);
            }
            else if (a == 64 || a == 120 || a == 123) fluid_synth_cc(synth.get(), 0, a, b);
            else if (a == 121) reset(bendRange);
        }
    }
    void render(float* left, float* right, int samples) noexcept
    {
        fluid_synth_write_float(synth.get(), samples, left, 0, 1, right, 0, 1);
        for (int i = 0; i < samples; ++i) {
            if (breathRemaining > 0) {
                breath += breathStep;
                if (--breathRemaining == 0) breath = targetBreath;
            }
            left[i] *= breath; right[i] *= breath;
        }
    }
    int voiceCount() const noexcept { return fluid_synth_get_active_voice_count(synth.get()); }
    void setBendRange(int semitones) noexcept {
        semitones = std::clamp(semitones, 0, 24);
        if(bendRange == semitones)return;
        bendRange = semitones;
        fluid_synth_pitch_wheel_sens(synth.get(), 0, bendRange);
    }
private:
    void disableDefaultAttenuation(int source, int sourceKind)
    {
        auto* mod = new_fluid_mod();
        if (!mod) throw std::bad_alloc();
        fluid_mod_set_source1(mod, source, sourceKind | FLUID_MOD_CONCAVE | FLUID_MOD_UNIPOLAR | FLUID_MOD_NEGATIVE);
        fluid_mod_set_source2(mod, 0, 0);
        fluid_mod_set_dest(mod, GEN_ATTENUATION); fluid_mod_set_amount(mod, 0);
        const auto result = fluid_synth_add_default_mod(synth.get(), mod, FLUID_SYNTH_OVERWRITE);
        delete_fluid_mod(mod);
        if (result != FLUID_OK) throw std::runtime_error("SF2 wind response setup failed");
    }
    void softenNewNote(int key) noexcept
    {
        // Bounded stack storage, no allocation. Only voices started by this
        // note are on for this key: earlier same-key voices have noteoff set.
        // Do not restart the envelope or dip the mix of an existing legato note.
        fluid_voice_t* voices[128]{};
        fluid_synth_get_voicelist(synth.get(), voices, 128, -1);
        constexpr float minimumAttackTimecents = -8358.941f; // 8 ms
        for (auto* voice : voices) {
            if (!voice) break;
            if (fluid_voice_get_key(voice) != key || !fluid_voice_is_on(voice)) continue;
            if (fluid_voice_gen_get(voice, GEN_VOLENVATTACK) < minimumAttackTimecents) {
                fluid_voice_gen_set(voice, GEN_VOLENVATTACK, minimumAttackTimecents);
                fluid_voice_update_param(voice, GEN_VOLENVATTACK);
            }
        }
        // Voice-level generator updates enqueue renderer events. Complete a
        // synth API transaction to publish them before rendering (FluidSynth
        // 2.6.3 flushes at API exit). This getter changes no controller state.
        int ignored = 0;
        fluid_synth_get_cc(synth.get(), 0, 11, &ignored);
    }
    struct SettingsDeleter { void operator()(fluid_settings_t* p) const { delete_fluid_settings(p); } };
    struct SynthDeleter { void operator()(fluid_synth_t* p) const { delete_fluid_synth(p); } };
    std::unique_ptr<fluid_settings_t, SettingsDeleter> settings;
    std::unique_ptr<fluid_synth_t, SynthDeleter> synth;
    int soundfont = -1;
    int bendRange = 2;
    int rampSamples = 1, breathRemaining = 0;
    float breath = 0.f, targetBreath = 0.f, breathStep = 0.f;
};
}

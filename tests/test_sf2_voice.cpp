#include "Sf2Voice.h"
#include <array>
#include <iostream>

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    int failures = 0;
    auto check = [&](bool ok, const char* reason) { if (!ok) { ++failures; std::cerr << reason << '\n'; } };
    for (double rate : {44100., 48000., 96000.}) for (int block : {64, 128, 480, 512})
    {
        fengyin::sf2::Voice voice(rate);
        const auto presets = voice.load(argv[1]);
        check(presets.size() == 1 && presets[0].bank == 0 && presets[0].program == 0, "Preset enumeration");
        check(!voice.select(999, 127), "Unknown preset rejected");
        check(voice.select(0, 0), "Preset selection");
        auto send = [&](int status, int a, int b) {
            unsigned char bytes[] {static_cast<unsigned char>(status), static_cast<unsigned char>(a), static_cast<unsigned char>(b)};
            voice.midi(bytes, 3);
        };
        auto level = [&] {
            std::array<float, 512> left{}, right{};
            double squared = 0; int count = 0;
            for (int n = 0; n < int(rate / block / 2); ++n) {
                voice.render(left.data(), right.data(), block);
                for (int k = 0; k < block; ++k) {
                    check(std::isfinite(left[k]) && std::isfinite(right[k]), "Finite output");
                    if (n > int(rate / block / 4)) { squared += double(left[k]) * left[k]; ++count; }
                }
            }
            return std::sqrt(squared / std::max(1,count));
        };
        send(0x90, 69, 100);
        check(level() < .00001, "Cold NoteOn without breath is silent");
        send(0xb0, 11, 100); const auto first = level();
        check(first > .005, "Breath produces actual PCM");
        for (int note : {71, 72}) {
            send(0x90, note, 100); send(0x80, note == 71 ? 69 : 71, 0);
            send(0xb0, 11, 40); const auto weak = level();
            check(weak > 0 && weak < first * .5, "Legato directly follows decreasing CC11");
            send(0xb0, 11, 100); check(level() > weak * 2, "Legato can increase again");
        }
        send(0xb0, 11, 0); check(level() < .00001, "Zero expression is silent");
        send(0xb0, 11, 127); send(0xb0, 120, 0); check(level() < .00001, "AllSoundOff releases all voices");
        voice.reset(); send(0x90, 69, 100); check(level() < .00001, "Reset does not retain breath");
        send(0xb0,11,100); const auto before = level();
        send(0xb1,11,0); check(std::abs(level()-before) < before*.03, "Other channels cannot alter expression");
        send(0xb0,2,0); send(0xb0,7,0); check(std::abs(level()-before) < before*.03, "Unknown/second breath CC cannot double-control");
        send(0x90,69,0); check(level() < .00001, "Velocity zero releases note");
        // Same-key rearticulation and sustain must not leak a held voice.
        for(int i=0;i<32;++i){send(0x90,69,90);send(0x90,69,90);send(0x80,69,0);}
        check(level()<.00001,"Repeated same-key release is complete");
        send(0xb0,64,127);send(0x90,69,90);send(0x80,69,0);
        check(level()>.001,"Sustain preserves released note");
        send(0xb0,64,0);check(level()<.00001,"Sustain release stops note");
    }
    std::cout << "SF2 PCM tests failures=" << failures << '\n';
    return failures ? 1 : 0;
}

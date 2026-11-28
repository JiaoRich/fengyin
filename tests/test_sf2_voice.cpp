#include "Sf2Voice.h"
#include <array>
#include <iostream>

int main(int argc, char** argv)
{
    if (argc != 2 && argc != 3) return 2;
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
        // Each phrase can begin at velocity 1; changing breath later must
        // recover full expression without requiring another note.
        double reference = 0;
        for (int velocity : {100,1,5,20,64,127,1}) {
            voice.reset(); send(0x90,69,velocity);
            check(level()==0,"No breath means exact silence");
            send(0xb0,11,100); const auto full=level();
            if (!reference) reference=full;
            check(std::abs(full-reference)<reference*.02,"Attack velocity cannot lock wind amplitude");
            send(0xb0,11,50); const auto half=level();
            check(std::abs(half/full-.5)<.02,"Single linear breath amplitude, no double attenuation");
            send(0xb0,11,1);check(level()>0,"No positive breath threshold gate");
            send(0xb0,11,0);check(level()==0,"Held phrase reaches exact silence");
            send(0x80,69,0);check(level()==0,"Stopped phrase remains silent");
        }
    }
    if (argc == 3) for (double rate : {44100.,48000.,96000.}) {
        fengyin::sf2::Voice v(rate);v.load(argv[2]);check(v.select(0,0),"Fast attack fixture");
        auto send=[&](int s,int a,int b){unsigned char d[]{(unsigned char)s,(unsigned char)a,(unsigned char)b};v.midi(d,3);};
        std::vector<float> l(size_t(rate/10)),r(l.size());
        send(0xb0,11,127);v.render(l.data(),r.data(),int(l.size())); // settled breath before note
        send(0x90,69,127);v.render(l.data(),r.data(),int(l.size()));
        auto rms=[&](int a,int b){double e=0;for(int i=a;i<b;++i)e+=l[size_t(i)]*l[size_t(i)];return std::sqrt(e/(b-a));};
        const auto onset=rms(0,int(rate*.002)),steady=rms(int(rate*.05),int(rate*.09));
        std::cout<<"SF2 softened onset rate="<<rate<<" first2ms/steady="<<onset/steady<<'\n';
        check(onset<steady*.4 && steady>.001,"Fast sample gets bounded soft onset, no missing note");
        // A new legato note does not reapply a phrase-wide attack to the old voice.
        send(0x90,71,100);v.render(l.data(),r.data(),int(l.size()));
        check(rms(0,int(rate*.002))>steady*.5,"Legato does not dip the existing voice");
    }
    std::cout << "SF2 PCM tests failures=" << failures << '\n';
    return failures ? 1 : 0;
}

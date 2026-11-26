#include "BlePairingPolicy.h"
#include <iostream>
int main()
{
    using fengyin::useYdsCustomPairing;
    int failures=0;
    const auto check=[&](bool value) { if(!value)++failures; };
    check(useYdsCustomPairing("YDS-150 MIDI","connect",false));
    check(useYdsCustomPairing("YDS-150","repair",false));
    check(!useYdsCustomPairing("YDS-150 MIDI","connect",true));
    check(!useYdsCustomPairing("YDS-150 MIDI","reconnect",false));
    for(const char* name : {"oodi C2","C20","XR9000","EWI Solo","YDS-120","YDS-150 audio","unrelated"})
        check(!useYdsCustomPairing(name,"connect",false));
    std::cout<<"Pairing scope failures="<<failures<<'\n';return failures?1:0;
}

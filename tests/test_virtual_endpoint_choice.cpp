#include "../audio-engine/common/VirtualEndpointChoice.h"
#include <cassert>
int main()
{
    using fengyin::audioengine::matchesVirtualEndpoint;
    assert(matchesVirtualEndpoint(L"Speakers (FengYin)", false));
    assert(matchesVirtualEndpoint(L"风吟共享扬声器", false));
    assert(matchesVirtualEndpoint(L"CABLE Input (VB-Audio Virtual Cable)", true));
    assert(matchesVirtualEndpoint(L"CABLE Output (VB-Audio Virtual Cable)", true, true));
    assert(!matchesVirtualEndpoint(L"CABLE Input (VB-Audio Virtual Cable)", true, true));
    assert(!matchesVirtualEndpoint(L"CABLE Output (VB-Audio Virtual Cable)", true));
    assert(!matchesVirtualEndpoint(L"CABLE-A Input (VB-Audio Cable A)", true));
    assert(!matchesVirtualEndpoint(L"Speakers (Realtek Audio)", true));
    assert(!matchesVirtualEndpoint(L"Microphone (Realtek Audio)", true, true));
    assert(!matchesVirtualEndpoint(L"Voicemeeter Input", true));
    assert(matchesVirtualEndpoint(L"扬声器 (VB-Audio Virtual Cable)", true));
    assert(matchesVirtualEndpoint(L"Speakers (VB-Audio Virtual Cable)", true));
    assert(!matchesVirtualEndpoint(L"CABLE In 16 Ch (VB-Audio Virtual Cable)", true));
    assert(!matchesVirtualEndpoint(L"风吟共享扬声器", true));
    assert(!matchesVirtualEndpoint(L"CABLE Input (VB-Audio Virtual Cable)", false));
    return 0;
}

#include "BluetoothMidiService.h"
#include <cassert>
#include <iostream>
int main()
{
    juce::MessageManager::getInstance();
    for(int cycle=0;cycle<8;++cycle)
    {
        fengyin::BluetoothMidiService service(false); // Never read/write real pairing history in a test.
        juce::Thread::sleep(40);
        auto state=service.snapshot();
        assert(state["devices"].isArray() && state["devices"].getArray()->isEmpty());
        assert(state["connectedId"].toString().isEmpty());
        // Unknown endpoint IDs cannot open, unpair, or impersonate an actual device.
        service.command("connect","not-a-discovered-device");
        service.command("forget","not-a-discovered-device");
        service.command("cancel");
        service.command("disconnect");
        juce::Thread::sleep(40);
        assert(service.snapshot()["connectedId"].toString().isEmpty());
        service.shutdown();service.shutdown(); // Safe repeated stop while commands are queued.
    }
    juce::MessageManager::deleteInstance();
    std::cout << "Bluetooth control lifecycle, invalid identity and shutdown checks passed\n";
}

#include "BluetoothMidiService.h"
#include "BleMidiConnectionPolicy.h"
#include <cassert>
#include <iostream>
#include <string>

static void testOnDemandPairing()
{
    using namespace fengyin;
    static_assert(isBleMidiAuthenticationError(0x05) && isBleMidiAuthenticationError(0x0c)
                  && isBleMidiAuthenticationError(0x0f));
    static_assert(!isBleMidiAuthenticationError(0x00) && !isBleMidiAuthenticationError(0x08));
    std::string calls;
    const auto pair = [&](const BleMidiAuthenticationRequired&) { calls += 'P'; };
    openBleMidiWithPairing([&] { calls += 'G'; }, pair);
    assert(calls == "G");

    calls.clear();
    openBleMidiWithPairing([&] {
        calls += 'G';
        if(calls == "G")throw BleMidiAuthenticationRequired("requires encryption");
    }, pair);
    assert(calls == "GPG");

    calls.clear();
    try {
        openBleMidiWithPairing([&] { calls += 'G'; throw std::runtime_error("device unreachable"); }, pair);
        assert(false);
    } catch(const std::runtime_error&) { assert(calls == "G"); }

    calls.clear();
    try {
        openBleMidiWithPairing([&] { calls += 'G'; throw BleMidiAuthenticationRequired("requires encryption"); },
            [&](const auto&) { calls += 'P'; throw std::runtime_error("user declined pairing"); });
        assert(false);
    } catch(const std::runtime_error&) { assert(calls == "GP"); }

    calls.clear();
    try {
        openBleMidiWithPairing([&] { calls += 'G'; throw BleMidiAuthenticationRequired("still requires encryption"); }, pair);
        assert(false);
    } catch(const BleMidiAuthenticationRequired&) { assert(calls == "GPG"); }
}

int main()
{
    testOnDemandPairing();
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
    std::cout << "Bluetooth GATT-first, authentication-only pairing, one retry, cancellation and control lifecycle checks passed\n";
}

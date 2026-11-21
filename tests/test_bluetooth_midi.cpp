#ifndef FENGYIN_BLE_POLICY_ONLY
#include "BluetoothMidiService.h"
#endif
#include "BleMidiConnectionPolicy.h"
#include <cassert>
#include <iostream>
#include <string>
#include <thread>
#include <future>

static void testOnDemandPairing()
{
    using namespace fengyin;
    static_assert(isBleMidiAuthenticationError(0x05) && isBleMidiAuthenticationError(0x0c)
                  && isBleMidiAuthenticationError(0x0f));
    static_assert(!isBleMidiAuthenticationError(0x00) && !isBleMidiAuthenticationError(0x08));
    std::string calls;
    const auto unpaired = [] { return false; };
    const auto close = [] {};
    const auto pair = [&](const BleMidiAuthenticationRequired&) { calls += 'P'; };
    openBleMidiWithPairing([&] { calls += 'G'; }, unpaired, close, pair);
    assert(calls == "G");

    calls.clear();
    openBleMidiWithPairing([&] {
        calls += 'G';
        if(calls == "G")throw BleMidiAuthenticationRequired("requires encryption");
    }, unpaired, close, pair);
    assert(calls == "GPG");

    calls.clear();
    try {
        openBleMidiWithPairing([&] { calls += 'G'; throw std::runtime_error("device unreachable"); }, unpaired, close, pair);
        assert(false);
    } catch(const std::runtime_error&) { assert(calls == "G"); }

    calls.clear();
    try {
        openBleMidiWithPairing([&] { calls += 'G'; throw BleMidiAuthenticationRequired("requires encryption"); }, unpaired, close,
            [&](const auto&) { calls += 'P'; throw std::runtime_error("user declined pairing"); });
        assert(false);
    } catch(const std::runtime_error&) { assert(calls == "GP"); }

    calls.clear();
    try {
        openBleMidiWithPairing([&] { calls += 'G'; throw BleMidiAuthenticationRequired("still requires encryption"); }, unpaired, close, pair);
        assert(false);
    } catch(const BleMidiAuthenticationRequired&) { assert(calls == "GPG"); }
}


static void testPairedAuthenticationRecovery()
{
    using namespace fengyin;
    std::string calls;
    const auto paired=[] { return true; };
    const auto close=[&] { calls+='C'; };
    const auto pair=[&](const auto&) { calls+='P'; };
    openBleMidiWithPairing([&]{calls+='G';},paired,close,pair);
    assert(calls=="G"); // Valid bonds never cause a pairing dialog.
    calls.clear();
    openBleMidiWithPairing([&]{calls+='G';if(calls=="G")throw BleMidiAuthenticationRequired("stale handle");},paired,close,pair);
    assert(calls=="GCG");
    calls.clear();
    try {
        openBleMidiWithPairing([&]{calls+='G';throw BleMidiAuthenticationRequired("bond unusable");},paired,close,pair);
        assert(false);
    } catch(const BleMidiRepairRequired&) {assert(calls=="GCG");}
    calls.clear();
    try {
        openBleMidiWithPairing([&]{calls+='G';if(calls=="G")throw BleMidiAuthenticationRequired("auth");throw std::runtime_error("unreachable");},paired,close,pair);
        assert(false);
    } catch(const BleMidiRepairRequired&) {assert(false);}
    catch(const std::runtime_error&) {assert(calls=="GCG");} // A timeout is not permission to unpair.
}

static void testLinkEpochAndRecovery()
{
    using namespace fengyin;
    BleMidiLinkEvents events;
    auto first=events.current();
    std::promise<void> passedEarlyGuard, resume;
    auto gate=resume.get_future();
    std::thread oldCallback([&]{
        assert(events.current()==first);
        passedEarlyGuard.set_value();gate.wait();
        assert(!events.post(first,BleMidiLinkEvents::receiveError,123));
    });
    passedEarlyGuard.get_future().wait();
    events.reset();
    auto second=events.current();
    assert(events.post(second,BleMidiLinkEvents::disconnected));
    resume.set_value();oldCallback.join();
    auto event=events.take();
    assert(event && event->epoch==second && event->causes==BleMidiLinkEvents::disconnected && event->error==0);
    assert(!events.take());
    assert(events.post(second,BleMidiLinkEvents::receiveError,42));
    assert(events.post(second,BleMidiLinkEvents::sessionClosed,7));
    event=events.take();assert(event && event->causes==10 && event->error==42);
    events.reset();assert(!events.post(second,BleMidiLinkEvents::disconnected));assert(!events.take());

    BleMidiRecoveryBudget budget;
    budget.newIntent();budget.connected(0);budget.lost(500);
    assert(budget.beginAttempt());budget.connected(1000);budget.lost(1500);
    assert(budget.beginAttempt());budget.connected(2000);budget.lost(2500);
    assert(!budget.beginAttempt() && !budget.available()); // No endless reconnect loop.
    budget.connected(3000);budget.lost(33000);
    assert(budget.beginAttempt() && budget.beginAttempt() && !budget.beginAttempt());
    budget.newIntent();assert(budget.beginAttempt());

    // A slow initial GATT phase does not steal the confirmation/post-pair budget.
    const auto hard=double(bleMidiRequestTimeoutMs);
    assert(bleMidiStageDeadline(35000,hard,bleMidiConnectionTimeoutMs)==80000);
    assert(bleMidiStageDeadline(79000,hard,bleMidiConnectionTimeoutMs)==124000);
    assert(bleMidiStageDeadline(125000,hard,bleMidiConnectionTimeoutMs)==hard);
    assert(bleMidiStageDeadline(100,20100,bleMidiRecoveryTimeoutMs)==20100);
}

int main()
{
    testOnDemandPairing();
    testPairedAuthenticationRecovery();
    testLinkEpochAndRecovery();
#ifndef FENGYIN_BLE_POLICY_ONLY
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
        service.command("repair","not-a-discovered-device");
        service.command("cancel");
        service.command("disconnect");
        juce::Thread::sleep(40);
        assert(service.snapshot()["connectedId"].toString().isEmpty());
        service.shutdown();service.shutdown(); // Safe repeated stop while commands are queued.
    }
    juce::MessageManager::deleteInstance();
#endif
    std::cout << "Bluetooth GATT-first, authentication recovery, epoch isolation, bounded retry, stage deadlines and control lifecycle checks passed\n";
}

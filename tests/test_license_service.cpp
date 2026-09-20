#include "LicenseService.h"
#include <cassert>
int main()
{
    juce::RSAKey publicKey, privateKey;
    const int seeds[] { 1729, 8191, 65537, 104729, 1299709 };
    juce::RSAKey::createKeyPair(publicKey, privateKey, 512, seeds, 5);
    const auto machine = fengyin::LicenseService::createMachineCode({ "disk-B", "device-A" });
    assert(machine == fengyin::LicenseService::createMachineCode({ "device-A", "disk-B" }));
    assert(machine.length() == 24);
    const auto code = fengyin::LicenseService::createActivationCode(machine, "FY-000001", privateKey);
    fengyin::LicenseService service(publicKey.toString());
    const auto valid = service.validate(code, machine);
    assert(valid.activated && valid.licenseId == "FY-000001");
    assert(! service.validate(code, "AAAA-BBBB-CCCC-DDDD-EEEE").activated);
    assert(! service.validate(code + "1", machine).activated);

    const auto trialFolder = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("fengyin-trial-test", {}, true);
    assert(trialFolder.createDirectory());
    juce::int64 now = 1'800'000'000'000LL;
    fengyin::LicenseService trialService(publicKey.toString(), trialFolder, [&now] { return now; });
    const auto notStarted = trialService.getStatus();
    assert(! notStarted.canUseFeatures() && ! notStarted.trialExpired);
    const auto started = trialService.startTrial();
    assert(started.trialActive && started.canUseFeatures());
    assert(started.trialRemainingSeconds == 3 * 24 * 60 * 60);

    now += 71LL * 60 * 60 * 1000;
    assert(trialService.getStatus().trialActive);
    now += 2LL * 60 * 60 * 1000;
    const auto expired = trialService.getStatus();
    assert(expired.trialExpired && ! expired.canUseFeatures());

    fengyin::LicenseService restarted(publicKey.toString(), trialFolder, [&now] { return now; });
    assert(restarted.getStatus().trialExpired);
    trialFolder.deleteRecursively();
    return 0;
}

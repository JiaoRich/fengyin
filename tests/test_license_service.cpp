#include "LicenseService.h"
#include <cassert>
#include <iostream>
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

    // RSA integers discard leading hexadecimal zeroes. A valid SHA256 digest
    // may start with zero, independently of the OS/hardware that was signed.
    bool checkedLeadingZero = false;
    for (int index = 0; index < 4096; ++index)
    {
        const auto id = "ZERO-" + juce::String(index);
        const auto payload = "FY1|" + machine.removeCharacters("-") + "|" + id + "|PERMANENT";
        const auto digest = juce::SHA256(payload.toRawUTF8(), static_cast<size_t>(payload.getNumBytesAsUTF8())).toHexString();
        if (! digest.startsWithChar('0')) continue;
        const auto signedCode = fengyin::LicenseService::createActivationCode(machine, id, privateKey);
        const auto result = service.validate(signedCode, machine);
        if (! result.activated)
        {
            std::cerr << "Leading-zero digest rejected: " << digest << " / " << result.message << '\n';
            return 1;
        }
        assert(! service.validate(signedCode + "1", machine).activated);
        checkedLeadingZero = true;
        break;
    }
    assert(checkedLeadingZero);

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

    // New licences use firmware identity, not the filesystem/OS installation.
    const auto hardware = juce::SystemStats::getUniqueDeviceID().trim();
    assert(hardware.isNotEmpty());
    const auto hardwareMachine = fengyin::LicenseService::createMachineCode({ "FY-HARDWARE-2", hardware });
    assert(restarted.getMachineCode() == hardwareMachine);
    const auto hardwareCode = fengyin::LicenseService::createActivationCode(hardwareMachine, "HW-TEST", privateKey);
    assert(restarted.activate(hardwareCode).activated);
    fengyin::LicenseService reopened(publicKey.toString(), trialFolder, [&now] { return now; });
    assert(reopened.getStatus().activated);

    // Upgrade must not revoke an old signed licence on the same installation.
    using Flags = juce::SystemStats::MachineIdFlags;
    auto legacyIds = juce::SystemStats::getMachineIdentifiers(Flags::uniqueId | Flags::fileSystemId);
    if (legacyIds.isEmpty()) legacyIds.add(juce::SystemStats::getComputerName());
    const auto legacyMachine = fengyin::LicenseService::createMachineCode(legacyIds);
    const auto legacyCode = fengyin::LicenseService::createActivationCode(legacyMachine, "LEGACY-TEST", privateKey);
    assert(reopened.activate(legacyCode).activated);
    assert(reopened.getStatus().licenseId == "LEGACY-TEST");
    trialFolder.deleteRecursively();
    return 0;
}

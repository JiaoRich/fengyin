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
    return 0;
}

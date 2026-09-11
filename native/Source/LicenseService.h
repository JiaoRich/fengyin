#pragma once
#include <juce_cryptography/juce_cryptography.h>
namespace fengyin
{
struct LicenseStatus { bool activated = false; juce::String message; juce::String licenseId; };
class LicenseService
{
public:
    explicit LicenseService(juce::String publicKeyText = {});
    juce::String getMachineCode() const;
    LicenseStatus getStatus() const;
    LicenseStatus activate(const juce::String& activationCode);
    static juce::String createMachineCode(const juce::StringArray& identifiers);
    static juce::String createActivationCode(const juce::String& machineCode, const juce::String& licenseId,
                                             const juce::RSAKey& privateKey);
    LicenseStatus validate(const juce::String& activationCode, const juce::String& machineCode) const;
private:
    juce::File getLicenseFile() const;
    bool saveCode(const juce::String&) const;
    juce::RSAKey publicKey;
};
}

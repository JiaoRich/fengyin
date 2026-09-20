#pragma once
#include <juce_cryptography/juce_cryptography.h>
#include <functional>
namespace fengyin
{
struct LicenseStatus
{
    bool activated = false;
    bool trialActive = false;
    bool trialExpired = false;
    juce::int64 trialRemainingSeconds = 0;
    juce::String message;
    juce::String licenseId;

    [[nodiscard]] bool canUseFeatures() const noexcept { return activated || trialActive; }
};
class LicenseService
{
public:
    explicit LicenseService(juce::String publicKeyText = {}, juce::File storageDirectory = {},
                            std::function<juce::int64()> currentTimeMillis = {});
    juce::String getMachineCode() const;
    LicenseStatus getStatus();
    LicenseStatus startTrial();
    LicenseStatus activate(const juce::String& activationCode);
    static juce::String createMachineCode(const juce::StringArray& identifiers);
    static juce::String createActivationCode(const juce::String& machineCode, const juce::String& licenseId,
                                             const juce::RSAKey& privateKey);
    LicenseStatus validate(const juce::String& activationCode, const juce::String& machineCode) const;
public: // Exposed for deterministic persistence tests; not part of the customer-facing API.
    struct TrialRecord
    {
        juce::String machineCode;
        juce::int64 startedAtMs = 0;
        juce::int64 lastSeenAtMs = 0;
        bool expired = false;
    };

private:
    juce::File getStorageDirectory() const;
    juce::File getLicenseFile() const;
    juce::Array<juce::File> getTrialFiles() const;
    std::optional<TrialRecord> readTrialRecord(const juce::File&) const;
    bool saveTrialRecord(const TrialRecord&) const;
    LicenseStatus getTrialStatus();
    juce::int64 nowMillis() const;
    bool saveCode(const juce::String&) const;
    juce::RSAKey publicKey;
    juce::File storageDirectoryOverride;
    std::function<juce::int64()> timeProvider;
};
}

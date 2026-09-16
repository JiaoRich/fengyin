#pragma once

#include <juce_core/juce_core.h>
#include <optional>
#include <vector>

namespace fengyin
{
struct ActivationRecord
{
    juce::String id;
    juce::String machineCode;
    juce::String activationCode;
    juce::String licenseId;
    juce::String customerName;
    juce::String contact;
    juce::String orderSource;
    juce::String remark;
    juce::String createdAt;
};

class ActivationRecordStore
{
public:
    explicit ActivationRecordStore(juce::File storageDirectory = {});

    const std::vector<ActivationRecord>& records() const noexcept { return items; }
    std::optional<ActivationRecord> findByMachineCode(const juce::String& machineCode) const;
    bool add(const ActivationRecord& record);
    bool reload();
    bool exportCsv(const juce::File& destination) const;

    juce::File dataFile() const { return file; }
    static juce::String normaliseMachineCode(const juce::String& value);
    static juce::String formatMachineCode(const juce::String& value);
    static bool isValidMachineCode(const juce::String& value);
    static juce::String createLicenseId();

private:
    bool save() const;
    static juce::String childText(const juce::XmlElement& parent, const char* name);
    static void appendText(juce::XmlElement& parent, const char* name, const juce::String& value);
    static juce::String csvCell(const juce::String& value);

    juce::File file;
    std::vector<ActivationRecord> items;
};
}

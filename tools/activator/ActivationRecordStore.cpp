#include "ActivationRecordStore.h"

namespace fengyin
{
namespace
{
juce::File defaultStorageDirectory()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("FengYin")
        .getChildFile("Activator");
}
}

ActivationRecordStore::ActivationRecordStore(juce::File storageDirectory)
{
    if (storageDirectory == juce::File())
        storageDirectory = defaultStorageDirectory();
    file = storageDirectory.getChildFile("activation-records.xml");
    reload();
}

juce::String ActivationRecordStore::normaliseMachineCode(const juce::String& value)
{
    return value.retainCharacters("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz").toUpperCase();
}

juce::String ActivationRecordStore::formatMachineCode(const juce::String& value)
{
    const auto normalised = normaliseMachineCode(value);
    juce::StringArray groups;
    for (int i = 0; i < normalised.length(); i += 4)
        groups.add(normalised.substring(i, juce::jmin(i + 4, normalised.length())));
    return groups.joinIntoString("-");
}

bool ActivationRecordStore::isValidMachineCode(const juce::String& value)
{
    return normaliseMachineCode(value).length() == 20;
}

juce::String ActivationRecordStore::createLicenseId()
{
    return "FY-" + juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S-")
         + juce::Uuid().toString().substring(0, 6).toUpperCase();
}

std::optional<ActivationRecord> ActivationRecordStore::findByMachineCode(const juce::String& machineCode) const
{
    const auto wanted = normaliseMachineCode(machineCode);
    for (const auto& item : items)
        if (normaliseMachineCode(item.machineCode) == wanted)
            return item;
    return std::nullopt;
}

bool ActivationRecordStore::add(const ActivationRecord& record)
{
    if (! isValidMachineCode(record.machineCode) || record.activationCode.isEmpty())
        return false;
    if (findByMachineCode(record.machineCode).has_value())
        return false;
    items.insert(items.begin(), record);
    if (save())
        return true;
    items.erase(items.begin());
    return false;
}

bool ActivationRecordStore::reload()
{
    items.clear();
    if (! file.existsAsFile())
        return true;
    auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr || ! xml->hasTagName("fengyin-activation-records"))
        return false;

    for (auto* element : xml->getChildWithTagNameIterator("record"))
    {
        ActivationRecord record;
        record.id = element->getStringAttribute("id");
        record.createdAt = element->getStringAttribute("createdAt");
        record.machineCode = childText(*element, "machineCode");
        record.activationCode = childText(*element, "activationCode");
        record.licenseId = childText(*element, "licenseId");
        record.customerName = childText(*element, "customerName");
        record.contact = childText(*element, "contact");
        record.orderSource = childText(*element, "orderSource");
        record.remark = childText(*element, "remark");
        if (isValidMachineCode(record.machineCode) && record.activationCode.isNotEmpty())
            items.push_back(std::move(record));
    }
    return true;
}

bool ActivationRecordStore::save() const
{
    if (! file.getParentDirectory().createDirectory())
        return false;

    juce::XmlElement root("fengyin-activation-records");
    root.setAttribute("version", 1);
    for (const auto& record : items)
    {
        auto* element = root.createNewChildElement("record");
        element->setAttribute("id", record.id);
        element->setAttribute("createdAt", record.createdAt);
        appendText(*element, "machineCode", record.machineCode);
        appendText(*element, "activationCode", record.activationCode);
        appendText(*element, "licenseId", record.licenseId);
        appendText(*element, "customerName", record.customerName);
        appendText(*element, "contact", record.contact);
        appendText(*element, "orderSource", record.orderSource);
        appendText(*element, "remark", record.remark);
    }

    juce::TemporaryFile temporary(file);
    if (! root.writeTo(temporary.getFile()))
        return false;
    if (file.existsAsFile())
        file.copyFileTo(file.getSiblingFile("activation-records.backup.xml"));
    return temporary.overwriteTargetFileWithTemporary();
}

bool ActivationRecordStore::exportCsv(const juce::File& destination) const
{
    juce::String text = juce::String::charToString(0xfeff)
        + juce::String::fromUTF8("生成时间,客户昵称,联系方式,订单来源,机器码,内部授权编号,激活码,备注\n");
    for (const auto& record : items)
    {
        text += csvCell(record.createdAt) + "," + csvCell(record.customerName) + ","
              + csvCell(record.contact) + "," + csvCell(record.orderSource) + ","
              + csvCell(record.machineCode) + "," + csvCell(record.licenseId) + ","
              + csvCell(record.activationCode) + "," + csvCell(record.remark) + "\n";
    }
    return destination.replaceWithText(text);
}

juce::String ActivationRecordStore::childText(const juce::XmlElement& parent, const char* name)
{
    if (const auto* child = parent.getChildByName(name))
        return child->getAllSubText();
    return {};
}

void ActivationRecordStore::appendText(juce::XmlElement& parent, const char* name, const juce::String& value)
{
    auto* child = parent.createNewChildElement(name);
    child->addTextElement(value);
}

juce::String ActivationRecordStore::csvCell(const juce::String& value)
{
    return "\"" + value.replace("\"", "\"\"") + "\"";
}
}

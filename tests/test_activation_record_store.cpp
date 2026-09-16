#include "ActivationRecordStore.h"
#include <cassert>

int main()
{
    const auto directory = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("fengyin-activation-record-test", {}, true);
    assert(directory.createDirectory());

    fengyin::ActivationRecordStore store(directory);
    assert(store.records().empty());
    assert(fengyin::ActivationRecordStore::isValidMachineCode("D2A7-37F6-F038-FA6C-8F06"));
    assert(! fengyin::ActivationRecordStore::isValidMachineCode("D2A7-37F6"));

    fengyin::ActivationRecord record;
    record.id = juce::Uuid().toString();
    record.machineCode = "D2A7-37F6-F038-FA6C-8F06";
    record.activationCode = "a-very-long-activation-code";
    record.licenseId = fengyin::ActivationRecordStore::createLicenseId();
    record.customerName = juce::String::fromUTF8("管友 🎷 / 任意符号 <> & \"'");
    record.contact = juce::String::repeatedString("微信-电话-任意文字-", 900);
    record.orderSource = {};
    record.remark = juce::String::fromUTF8("第一行\n第二行；无需限制内容。\n☎ 138 0000 0000");
    record.createdAt = juce::Time::getCurrentTime().toISO8601(true);
    assert(store.add(record));
    assert(! store.add(record));

    fengyin::ActivationRecordStore reloaded(directory);
    assert(reloaded.records().size() == 1);
    const auto found = reloaded.findByMachineCode("d2a737f6f038fa6c8f06");
    assert(found.has_value());
    assert(found->customerName == record.customerName);
    assert(found->contact == record.contact);
    assert(found->orderSource.isEmpty());
    assert(found->remark == record.remark);

    const auto csv = directory.getChildFile(juce::String::fromUTF8("激活记录.csv"));
    assert(reloaded.exportCsv(csv));
    const auto exported = csv.loadFileAsString();
    assert(exported.contains(record.customerName));
    assert(exported.contains(juce::String::fromUTF8("第一行")));
    assert(exported.contains(juce::String::fromUTF8("第二行；无需限制内容。")));
    assert(exported.contains(juce::String::fromUTF8("☎ 138 0000 0000")));

    assert(directory.deleteRecursively());
    return 0;
}

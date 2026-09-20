#include "LicenseService.h"
namespace fengyin
{
namespace
{
constexpr juce::int64 trialDurationSeconds = 3 * 24 * 60 * 60;
constexpr juce::int64 clockRollbackToleranceMs = 5 * 60 * 1000;
constexpr juce::int64 lastSeenWriteIntervalMs = 5 * 60 * 1000;
const auto trialSecret = juce::String("FengYin-Trial-2026-Offline-Guard");

juce::String normaliseMachineCode(juce::String code) { return code.retainCharacters("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ").toUpperCase(); }
juce::String digestFor(const juce::String& payload)
{
    return juce::SHA256(payload.toRawUTF8(), static_cast<size_t>(payload.getNumBytesAsUTF8())).toHexString();
}

juce::String trialPayload(const LicenseService::TrialRecord& record)
{
    return "FYTRIAL1|" + normaliseMachineCode(record.machineCode) + "|"
         + juce::String(record.startedAtMs) + "|" + juce::String(record.lastSeenAtMs) + "|"
         + (record.expired ? "1" : "0");
}
}
LicenseService::LicenseService(juce::String publicKeyText, juce::File storageDirectory,
                               std::function<juce::int64()> currentTimeMillis)
    : publicKey(publicKeyText), storageDirectoryOverride(std::move(storageDirectory)),
      timeProvider(std::move(currentTimeMillis)) {}
juce::String LicenseService::createMachineCode(const juce::StringArray& sourceIdentifiers)
{
    auto identifiers = sourceIdentifiers;
    identifiers.removeEmptyStrings();
    identifiers.sort(true);
    const auto stableInput = identifiers.joinIntoString("|");
    const auto hash = juce::SHA256(stableInput.toRawUTF8(), static_cast<size_t>(stableInput.getNumBytesAsUTF8()))
                          .toHexString().toUpperCase().substring(0, 20);
    juce::StringArray groups;
    for (int i = 0; i < hash.length(); i += 4) groups.add(hash.substring(i, i + 4));
    return groups.joinIntoString("-");
}
juce::String LicenseService::getMachineCode() const
{
    using Flags = juce::SystemStats::MachineIdFlags;
    auto identifiers = juce::SystemStats::getMachineIdentifiers(Flags::uniqueId | Flags::fileSystemId);
    if (identifiers.isEmpty()) identifiers.add(juce::SystemStats::getComputerName());
    return createMachineCode(identifiers);
}
juce::String LicenseService::createActivationCode(const juce::String& machineCode, const juce::String& licenseId,
                                                   const juce::RSAKey& privateKey)
{
    if (! privateKey.isValid()) return {};
    const auto payload = "FY1|" + normaliseMachineCode(machineCode) + "|"
                       + licenseId.retainCharacters("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-") + "|PERMANENT";
    juce::BigInteger signature;
    signature.parseString(digestFor(payload), 16);
    if (! privateKey.applyToValue(signature)) return {};
    return juce::Base64::toBase64(payload) + "." + signature.toString(16);
}
LicenseStatus LicenseService::validate(const juce::String& rawCode, const juce::String& machineCode) const
{
    LicenseStatus result;
    if (! publicKey.isValid()) { result.message = juce::String::fromUTF8("当前版本尚未配置正式授权公钥"); return result; }
    const auto code = rawCode.removeCharacters("\r\n \t");
    const auto separator = code.indexOfChar('.');
    if (separator <= 0 || separator >= code.length() - 1) { result.message = juce::String::fromUTF8("激活码格式不正确"); return result; }
    juce::MemoryOutputStream decoded;
    if (! juce::Base64::convertFromBase64(decoded, code.substring(0, separator)))
    { result.message = juce::String::fromUTF8("激活码内容无法识别"); return result; }
    const auto payload = decoded.toString();
    const auto fields = juce::StringArray::fromTokens(payload, "|", "");
    if (fields.size() != 4 || fields[0] != "FY1" || fields[3] != "PERMANENT")
    { result.message = juce::String::fromUTF8("激活码版本不受支持"); return result; }
    if (fields[1] != normaliseMachineCode(machineCode))
    { result.message = juce::String::fromUTF8("此激活码不属于当前电脑"); return result; }
    juce::BigInteger verified;
    verified.parseString(code.substring(separator + 1), 16);
    if (verified.isZero() || ! publicKey.applyToValue(verified) || verified.toString(16) != digestFor(payload))
    { result.message = juce::String::fromUTF8("激活码签名无效，请检查是否复制完整"); return result; }
    result.activated = true;
    result.licenseId = fields[2];
    result.message = juce::String::fromUTF8("已永久激活");
    return result;
}
LicenseStatus LicenseService::getStatus()
{
    const auto file = getLicenseFile();
    if (file.existsAsFile())
    {
        auto permanent = validate(file.loadFileAsString(), getMachineCode());
        if (permanent.activated) return permanent;
    }
    return getTrialStatus();
}
LicenseStatus LicenseService::startTrial()
{
    auto status = getStatus();
    if (status.activated || status.trialActive || status.trialExpired)
        return status;

    const auto now = nowMillis();
    TrialRecord record { getMachineCode(), now, now, false };
    if (! saveTrialRecord(record))
        return { false, false, false, 0, juce::String::fromUTF8("无法保存试用信息，请检查系统磁盘权限"), {} };
    return getTrialStatus();
}
LicenseStatus LicenseService::activate(const juce::String& activationCode)
{
    auto status = validate(activationCode, getMachineCode());
    if (status.activated && ! saveCode(activationCode.removeCharacters("\r\n \t")))
    { status.activated = false; status.message = juce::String::fromUTF8("激活码正确，但无法保存到本机"); }
    return status;
}
juce::File LicenseService::getLicenseFile() const
{
    return getStorageDirectory().getChildFile("license.dat");
}
bool LicenseService::saveCode(const juce::String& code) const
{
    const auto file = getLicenseFile();
    if (! file.getParentDirectory().createDirectory()) return false;
    juce::TemporaryFile temporary(file);
    return temporary.getFile().replaceWithText(code) && temporary.overwriteTargetFileWithTemporary();
}

juce::File LicenseService::getStorageDirectory() const
{
    if (storageDirectoryOverride != juce::File()) return storageDirectoryOverride;
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("FengYin");
}

juce::Array<juce::File> LicenseService::getTrialFiles() const
{
    juce::Array<juce::File> files;
    files.add(getStorageDirectory().getChildFile("trial.dat"));
    if (storageDirectoryOverride == juce::File())
    {
        const auto shared = juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory)
                                .getChildFile("FengYin").getChildFile("trial.dat");
        if (shared != files[0]) files.add(shared);
    }
    return files;
}

std::optional<LicenseService::TrialRecord> LicenseService::readTrialRecord(const juce::File& file) const
{
    if (! file.existsAsFile()) return std::nullopt;
    juce::MemoryOutputStream decoded;
    if (! juce::Base64::convertFromBase64(decoded, file.loadFileAsString().removeCharacters("\r\n \t")))
        return TrialRecord { getMachineCode(), 1, nowMillis(), true };
    const auto fields = juce::StringArray::fromTokens(decoded.toString(), "|", "");
    if (fields.size() != 6 || fields[0] != "FYTRIAL1")
        return TrialRecord { getMachineCode(), 1, nowMillis(), true };

    TrialRecord record;
    record.machineCode = fields[1];
    record.startedAtMs = fields[2].getLargeIntValue();
    record.lastSeenAtMs = fields[3].getLargeIntValue();
    record.expired = fields[4] == "1";
    const auto payload = fields[0] + "|" + fields[1] + "|" + fields[2] + "|" + fields[3] + "|" + fields[4];
    if (record.machineCode != normaliseMachineCode(getMachineCode()) || record.startedAtMs <= 0
        || record.lastSeenAtMs < record.startedAtMs || fields[5] != digestFor(payload + "|" + trialSecret))
        return TrialRecord { getMachineCode(), 1, nowMillis(), true };
    return record;
}

bool LicenseService::saveTrialRecord(const TrialRecord& record) const
{
    const auto payload = trialPayload(record);
    const auto encoded = juce::Base64::toBase64(payload + "|" + digestFor(payload + "|" + trialSecret));
    bool savedPrimary = false;
    auto files = getTrialFiles();
    for (int index = 0; index < files.size(); ++index)
    {
        const auto& file = files.getReference(index);
        if (! file.getParentDirectory().createDirectory()) continue;
        juce::TemporaryFile temporary(file);
        const auto saved = temporary.getFile().replaceWithText(encoded)
                        && temporary.overwriteTargetFileWithTemporary();
        if (index == 0) savedPrimary = saved;
    }
    return savedPrimary;
}

LicenseStatus LicenseService::getTrialStatus()
{
    auto files = getTrialFiles();
    bool anyFile = false;
    std::optional<TrialRecord> combined;
    for (const auto& file : files)
    {
        if (! file.existsAsFile()) continue;
        anyFile = true;
        const auto record = readTrialRecord(file);
        if (! record.has_value()) continue;
        if (! combined.has_value()) combined = record;
        else
        {
            combined->startedAtMs = juce::jmin(combined->startedAtMs, record->startedAtMs);
            combined->lastSeenAtMs = juce::jmax(combined->lastSeenAtMs, record->lastSeenAtMs);
            combined->expired = combined->expired || record->expired;
        }
    }
    if (! anyFile)
        return { false, false, false, 0, juce::String::fromUTF8("可开始3天完整试用"), {} };
    if (! combined.has_value())
        return { false, false, true, 0, juce::String::fromUTF8("试用信息异常，请激活后继续使用"), {} };

    auto record = *combined;
    const auto now = nowMillis();
    const auto clockRolledBack = now + clockRollbackToleranceMs < record.lastSeenAtMs;
    const auto elapsedMs = juce::jmax<juce::int64>(0, now - record.startedAtMs);
    const auto remaining = juce::jmax<juce::int64>(0, trialDurationSeconds - elapsedMs / 1000);
    if (record.expired || clockRolledBack || remaining <= 0)
    {
        record.expired = true;
        record.lastSeenAtMs = juce::jmax(record.lastSeenAtMs, now);
        saveTrialRecord(record);
        return { false, false, true, 0,
                 clockRolledBack ? juce::String::fromUTF8("检测到系统时间异常，试用已停止，请激活后继续使用")
                                 : juce::String::fromUTF8("3天完整试用已结束，请激活后继续使用"), {} };
    }

    if (now - record.lastSeenAtMs >= lastSeenWriteIntervalMs)
    {
        record.lastSeenAtMs = now;
        saveTrialRecord(record);
    }
    return { false, true, false, remaining, juce::String::fromUTF8("3天完整试用中"), {} };
}

juce::int64 LicenseService::nowMillis() const
{
    return timeProvider ? timeProvider() : juce::Time::currentTimeMillis();
}
}

#include "LicenseService.h"
namespace fengyin
{
namespace
{
juce::String normaliseMachineCode(juce::String code) { return code.retainCharacters("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ").toUpperCase(); }
juce::String digestFor(const juce::String& payload)
{
    return juce::SHA256(payload.toRawUTF8(), static_cast<size_t>(payload.getNumBytesAsUTF8())).toHexString();
}
}
LicenseService::LicenseService(juce::String publicKeyText) : publicKey(publicKeyText) {}
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
LicenseStatus LicenseService::getStatus() const
{
    const auto file = getLicenseFile();
    if (! file.existsAsFile()) return { false, juce::String::fromUTF8("尚未激活"), {} };
    return validate(file.loadFileAsString(), getMachineCode());
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
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("FengYin").getChildFile("license.dat");
}
bool LicenseService::saveCode(const juce::String& code) const
{
    const auto file = getLicenseFile();
    if (! file.getParentDirectory().createDirectory()) return false;
    juce::TemporaryFile temporary(file);
    return temporary.getFile().replaceWithText(code) && temporary.overwriteTargetFileWithTemporary();
}
}

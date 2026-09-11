#include <juce_cryptography/juce_cryptography.h>
#include "LicenseService.h"
#include <iostream>
int main(int argc, char** argv)
{
    if (argc == 2 && juce::String(argv[1]) == "--create-key")
    {
        juce::RSAKey publicKey, privateKey;
        juce::RSAKey::createKeyPair(publicKey, privateKey, 1024);
        std::cout << "PUBLIC=" << publicKey.toString() << "\nPRIVATE=" << privateKey.toString() << "\n";
        return publicKey.isValid() && privateKey.isValid() ? 0 : 1;
    }
    if (argc != 4) { std::cerr << "用法: FengYinLicenseGenerator <机器码> <授权编号> <私钥文件>\n"; return 2; }
    const auto keyText = juce::File(juce::String(argv[3])).loadFileAsString().trim();
    const juce::RSAKey privateKey { keyText };
    const auto code = fengyin::LicenseService::createActivationCode(argv[1], argv[2], privateKey);
    if (code.isEmpty()) return 1;
    std::cout << code << "\n";
    return 0;
}

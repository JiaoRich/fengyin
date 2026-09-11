#include <juce_gui_extra/juce_gui_extra.h>
#include "LicenseService.h"

namespace
{
juce::String zh(const char* text) { return juce::String::fromUTF8(text); }

class ActivatorComponent final : public juce::Component
{
public:
    ActivatorComponent()
    {
        title.setText(zh("风吟 · 激活码工具"), juce::dontSendNotification);
        title.setFont(juce::FontOptions(28.0f, juce::Font::bold));
        title.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(title);

        configureLabel(machineLabel, "客户本机码");
        configureLabel(orderLabel, "订单编号（例如 FY-000001）");
        configureLabel(resultLabel, "生成的永久激活码");

        machineCode.setMultiLine(false);
        machineCode.setTextToShowWhenEmpty(zh("粘贴客户发来的五组本机码"), juce::Colour(0xff73879b));
        orderId.setMultiLine(false);
        orderId.setTextToShowWhenEmpty(zh("建议每位客户使用不同编号"), juce::Colour(0xff73879b));
        activationCode.setMultiLine(true);
        activationCode.setReadOnly(true);
        for (auto* editor : { &machineCode, &orderId, &activationCode })
        {
            editor->setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff102238));
            editor->setColour(juce::TextEditor::textColourId, juce::Colours::white);
            editor->setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff29445e));
            addAndMakeVisible(*editor);
        }

        generate.setButtonText(zh("生成永久激活码"));
        generate.onClick = [this] { generateCode(); };
        copy.setButtonText(zh("复制激活码"));
        copy.onClick = [this]
        {
            if (activationCode.getText().isNotEmpty())
                juce::SystemClipboard::copyTextToClipboard(activationCode.getText());
        };
        chooseKey.setButtonText(zh("选择私钥文件"));
        chooseKey.onClick = [this] { choosePrivateKey(); };
        addAndMakeVisible(generate);
        addAndMakeVisible(copy);
        addAndMakeVisible(chooseKey);

        status.setColour(juce::Label::textColourId, juce::Colour(0xff8fa7bd));
        addAndMakeVisible(status);
        locateDefaultKey();
        setSize(720, 520);
    }

    void paint(juce::Graphics& g) override { g.fillAll(juce::Colour(0xff07101d)); }

    void resized() override
    {
        auto area = getLocalBounds().reduced(28);
        title.setBounds(area.removeFromTop(45));
        status.setBounds(area.removeFromTop(32));
        machineLabel.setBounds(area.removeFromTop(26));
        machineCode.setBounds(area.removeFromTop(42));
        area.removeFromTop(14);
        orderLabel.setBounds(area.removeFromTop(26));
        orderId.setBounds(area.removeFromTop(42));
        area.removeFromTop(18);
        auto buttons = area.removeFromTop(42);
        generate.setBounds(buttons.removeFromLeft(210));
        buttons.removeFromLeft(10);
        copy.setBounds(buttons.removeFromLeft(150));
        buttons.removeFromLeft(10);
        chooseKey.setBounds(buttons.removeFromLeft(150));
        area.removeFromTop(18);
        resultLabel.setBounds(area.removeFromTop(26));
        activationCode.setBounds(area);
    }

private:
    void configureLabel(juce::Label& label, const char* text)
    {
        label.setText(zh(text), juce::dontSendNotification);
        label.setColour(juce::Label::textColourId, juce::Colour(0xffd8e8f5));
        addAndMakeVisible(label);
    }

    void locateDefaultKey()
    {
        auto besideApp = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                             .getParentDirectory().getChildFile("license-private-key.txt");
        if (besideApp.existsAsFile()) privateKeyFile = besideApp;
        refreshKeyStatus();
    }

    void refreshKeyStatus()
    {
        status.setText(privateKeyFile.existsAsFile()
                           ? zh("✓ 私钥已就绪（请勿把本工具或私钥发给客户）")
                           : zh("尚未找到私钥，请点击“选择私钥文件”"),
                       juce::dontSendNotification);
        status.setColour(juce::Label::textColourId,
                         privateKeyFile.existsAsFile() ? juce::Colour(0xff61e6a7) : juce::Colour(0xffffb86b));
    }

    void choosePrivateKey()
    {
        chooser = std::make_unique<juce::FileChooser>(zh("选择风吟激活私钥"), juce::File(), "*.txt");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& selected)
            {
                auto file = selected.getResult();
                if (file.existsAsFile()) privateKeyFile = file;
                refreshKeyStatus();
            });
    }

    void generateCode()
    {
        const auto machine = machineCode.getText().trim();
        const auto order = orderId.getText().trim();
        if (machine.isEmpty() || order.isEmpty() || ! privateKeyFile.existsAsFile())
        {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                zh("信息不完整"), zh("请填写客户本机码、订单编号，并确认私钥已就绪。"));
            return;
        }
        const juce::RSAKey key(privateKeyFile.loadFileAsString().trim());
        const auto code = fengyin::LicenseService::createActivationCode(machine, order, key);
        if (code.isEmpty())
        {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                zh("无法生成"), zh("私钥无效，请重新选择正确的私钥文件。"));
            return;
        }
        activationCode.setText(code, false);
        juce::SystemClipboard::copyTextToClipboard(code);
        status.setText(zh("✓ 已生成并复制，可以直接发给客户"), juce::dontSendNotification);
        status.setColour(juce::Label::textColourId, juce::Colour(0xff61e6a7));
    }

    juce::Label title, status, machineLabel, orderLabel, resultLabel;
    juce::TextEditor machineCode, orderId, activationCode;
    juce::TextButton generate, copy, chooseKey;
    juce::File privateKeyFile;
    std::unique_ptr<juce::FileChooser> chooser;
};

class ActivatorApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return zh("风吟激活码工具"); }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    void initialise(const juce::String&) override
    {
        window = std::make_unique<Window>(getApplicationName());
    }
    void shutdown() override { window.reset(); }

private:
    class Window final : public juce::DocumentWindow
    {
    public:
        explicit Window(const juce::String& name)
            : DocumentWindow(name, juce::Colour(0xff07101d), DocumentWindow::closeButton)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new ActivatorComponent(), true);
            centreWithSize(getWidth(), getHeight());
            setResizable(false, false);
            setVisible(true);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };
    std::unique_ptr<Window> window;
};
}

START_JUCE_APPLICATION(ActivatorApplication)

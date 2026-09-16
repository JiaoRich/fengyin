#include <juce_gui_extra/juce_gui_extra.h>
#include "ActivationRecordStore.h"
#include "LicenseService.h"

namespace
{
juce::String zh(const char* text) { return juce::String::fromUTF8(text); }

class ActivatorLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    ActivatorLookAndFeel()
    {
        setColour(juce::TextButton::buttonColourId, juce::Colour(0xff162b43));
        setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff176e91));
        setColour(juce::TextButton::textColourOffId, juce::Colour(0xffd9eafa));
        setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        setColour(juce::ScrollBar::thumbColourId, juce::Colour(0xff31536e));
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& colour,
                              bool highlighted, bool down) override
    {
        auto fill = highlighted ? colour.brighter(0.09f) : colour;
        if (down) fill = fill.darker(0.1f);
        g.setColour(fill);
        g.fillRoundedRectangle(button.getLocalBounds().toFloat(), 9.0f);
        g.setColour(juce::Colour(0xff31536e));
        g.drawRoundedRectangle(button.getLocalBounds().toFloat().reduced(0.5f), 9.0f, 1.0f);
    }
};

class ActivatorComponent final : public juce::Component, private juce::ListBoxModel
{
public:
    ActivatorComponent()
    {
        setLookAndFeel(&lookAndFeel);
        configureHeader();
        configureGeneratePage();
        configureRecordsPage();
        locateDefaultKey();
        refreshRecordFilter();
        showGeneratePage();
        setSize(1060, 720);
    }

    ~ActivatorComponent() override { setLookAndFeel(nullptr); }

    void paint(juce::Graphics& g) override
    {
        juce::ColourGradient background(juce::Colour(0xff07121f), 0.0f, 0.0f,
                                        juce::Colour(0xff0b1d31), 0.0f, static_cast<float>(getHeight()), false);
        g.setGradientFill(background);
        g.fillAll();
        g.setColour(juce::Colour(0x162fdcff));
        g.fillEllipse(static_cast<float>(getWidth() - 390), -210.0f, 540.0f, 540.0f);
        g.setColour(juce::Colour(0x102e70ff));
        g.fillEllipse(-190.0f, static_cast<float>(getHeight() - 250), 420.0f, 420.0f);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        auto header = bounds.removeFromTop(84).reduced(24, 13);
        logo.setBounds(header.removeFromLeft(52));
        header.removeFromLeft(12);
        auto brand = header.removeFromLeft(330);
        title.setBounds(brand.removeFromTop(34));
        subtitle.setBounds(brand);
        auto navigation = header.removeFromRight(250).reduced(0, 8);
        recordsTab.setBounds(navigation.removeFromRight(116));
        navigation.removeFromRight(10);
        generateTab.setBounds(navigation.removeFromRight(116));

        auto content = bounds.reduced(24, 12);
        generatePage.setBounds(content);
        recordsPage.setBounds(content);
        layoutGeneratePage();
        layoutRecordsPage();
    }

private:
    void configureHeader()
    {
        logo.setText(zh("风"), juce::dontSendNotification);
        logo.setJustificationType(juce::Justification::centred);
        logo.setFont(juce::FontOptions(26.0f, juce::Font::bold));
        logo.setColour(juce::Label::textColourId, juce::Colour(0xff61ddff));
        addAndMakeVisible(logo);
        title.setText(zh("风吟 · 激活管理"), juce::dontSendNotification);
        title.setFont(juce::FontOptions(27.0f, juce::Font::bold));
        title.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(title);
        subtitle.setText(zh("永久激活 · 本地保存 · 无需联网"), juce::dontSendNotification);
        subtitle.setFont(juce::FontOptions(13.0f));
        subtitle.setColour(juce::Label::textColourId, juce::Colour(0xff7893aa));
        addAndMakeVisible(subtitle);
        generateTab.setButtonText(zh("生成激活码"));
        recordsTab.setButtonText(zh("激活记录"));
        generateTab.onClick = [this] { showGeneratePage(); };
        recordsTab.onClick = [this] { showRecordsPage(); };
        addAndMakeVisible(generateTab);
        addAndMakeVisible(recordsTab);
    }

    void configureGeneratePage()
    {
        addAndMakeVisible(generatePage);
        configureSectionTitle(generateHeading, "生成永久激活码");
        generateIntro.setText(zh("粘贴客户机器码即可生成。其余信息全部可不填，也不限制字符或长度。"), juce::dontSendNotification);
        generateIntro.setColour(juce::Label::textColourId, juce::Colour(0xff8fa8bd));
        configureLabel(machineLabel, "机器码  ·  必填");
        configureLabel(customerLabel, "客户昵称  ·  选填");
        configureLabel(contactLabel, "联系方式  ·  选填");
        configureLabel(sourceLabel, "订单来源  ·  选填");
        configureLabel(remarkLabel, "备注  ·  选填");
        configureLabel(resultLabel, "永久激活码");
        configureEditor(machineCode, "例如 D2A7-37F6-F038-FA6C-8F06");
        configureEditor(customerName, "任意昵称或姓名");
        configureEditor(contact, "微信、手机号或其他任意联系方式");
        configureEditor(orderSource, "例如 微信、抖音、转介绍；也可留空");
        configureEditor(remark, "可输入任意长度的中文、数字、符号或多行说明", true);
        configureEditor(activationCode, "生成后显示在这里", true);
        activationCode.setReadOnly(true);
        keyStatus.setColour(juce::Label::textColourId, juce::Colour(0xffffbd72));
        generateButton.setButtonText(zh("生成、保存并复制"));
        generateButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1689b5));
        generateButton.onClick = [this] { generateCode(); };
        clearButton.setButtonText(zh("清空"));
        clearButton.onClick = [this] { clearForm(); };
        copyButton.setButtonText(zh("复制激活码"));
        copyButton.onClick = [this] { copyCurrentCode(); };
        chooseKeyButton.setButtonText(zh("选择私钥"));
        chooseKeyButton.onClick = [this] { choosePrivateKey(); };
        const std::array<juce::Component*, 19> components { &generateHeading, &generateIntro,
            &machineLabel, &customerLabel, &contactLabel, &sourceLabel, &remarkLabel,
            &resultLabel, &machineCode, &customerName, &contact, &orderSource, &remark,
            &activationCode, &keyStatus, &generateButton, &clearButton, &copyButton, &chooseKeyButton };
        for (auto* component : components)
            generatePage.addAndMakeVisible(*component);
    }

    void configureRecordsPage()
    {
        addAndMakeVisible(recordsPage);
        configureSectionTitle(recordsHeading, "激活记录");
        recordsCount.setColour(juce::Label::textColourId, juce::Colour(0xff7893aa));
        recordsCount.setJustificationType(juce::Justification::centredRight);
        configureEditor(search, "搜索客户、联系方式、订单来源、机器码或备注");
        search.onTextChange = [this] { refreshRecordFilter(); };
        exportButton.setButtonText(zh("导出 CSV 备份"));
        exportButton.onClick = [this] { exportRecords(); };
        recordList.setModel(this);
        recordList.setRowHeight(70);
        recordList.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff0a1828));
        recordList.setOutlineThickness(1);
        recordList.setColour(juce::ListBox::outlineColourId, juce::Colour(0xff213d56));
        configureEditor(recordDetails, "点击一条记录查看完整信息", true);
        recordDetails.setReadOnly(true);
        recordDetails.setFont(juce::FontOptions(14.0f));
        copyRecordButton.setButtonText(zh("复制所选激活码"));
        copyRecordButton.onClick = [this]
        {
            if (selectedRecord.has_value()) juce::SystemClipboard::copyTextToClipboard(selectedRecord->activationCode);
        };
        openFolderButton.setButtonText(zh("打开记录文件夹"));
        openFolderButton.onClick = [this] { store.dataFile().getParentDirectory().revealToUser(); };
        const std::array<juce::Component*, 8> components { &recordsHeading, &recordsCount, &search,
            &exportButton, &recordList, &recordDetails, &copyRecordButton, &openFolderButton };
        for (auto* component : components)
            recordsPage.addAndMakeVisible(*component);
    }

    void layoutGeneratePage()
    {
        auto area = generatePage.getLocalBounds().reduced(24, 20);
        generateHeading.setBounds(area.removeFromTop(38));
        generateIntro.setBounds(area.removeFromTop(30));
        area.removeFromTop(12);
        auto left = area.removeFromLeft((area.getWidth() - 20) * 3 / 5);
        area.removeFromLeft(20);
        auto right = area;
        machineLabel.setBounds(left.removeFromTop(24));
        machineCode.setBounds(left.removeFromTop(42));
        left.removeFromTop(12);
        auto pair = left.removeFromTop(70);
        auto first = pair.removeFromLeft((pair.getWidth() - 12) / 2);
        pair.removeFromLeft(12);
        customerLabel.setBounds(first.removeFromTop(24));
        customerName.setBounds(first);
        contactLabel.setBounds(pair.removeFromTop(24));
        contact.setBounds(pair);
        left.removeFromTop(12);
        sourceLabel.setBounds(left.removeFromTop(24));
        orderSource.setBounds(left.removeFromTop(42));
        left.removeFromTop(12);
        remarkLabel.setBounds(left.removeFromTop(24));
        remark.setBounds(left.removeFromTop(116));
        left.removeFromTop(16);
        auto actions = left.removeFromTop(44);
        generateButton.setBounds(actions.removeFromLeft(210));
        actions.removeFromLeft(10);
        clearButton.setBounds(actions.removeFromLeft(92));
        resultLabel.setBounds(right.removeFromTop(24));
        activationCode.setBounds(right.removeFromTop(220));
        right.removeFromTop(12);
        copyButton.setBounds(right.removeFromTop(44));
        right.removeFromTop(18);
        keyStatus.setBounds(right.removeFromTop(46));
        chooseKeyButton.setBounds(right.removeFromTop(40));
    }

    void layoutRecordsPage()
    {
        auto area = recordsPage.getLocalBounds().reduced(24, 20);
        auto heading = area.removeFromTop(42);
        recordsHeading.setBounds(heading.removeFromLeft(260));
        recordsCount.setBounds(heading);
        auto tools = area.removeFromTop(44);
        exportButton.setBounds(tools.removeFromRight(158));
        tools.removeFromRight(10);
        search.setBounds(tools);
        area.removeFromTop(14);
        auto listArea = area.removeFromLeft(area.getWidth() * 56 / 100);
        area.removeFromLeft(16);
        recordList.setBounds(listArea);
        auto detailActions = area.removeFromBottom(44);
        copyRecordButton.setBounds(detailActions.removeFromLeft(170));
        detailActions.removeFromLeft(10);
        openFolderButton.setBounds(detailActions.removeFromLeft(160));
        area.removeFromBottom(12);
        recordDetails.setBounds(area);
    }

    void configureSectionTitle(juce::Label& label, const char* text)
    {
        label.setText(zh(text), juce::dontSendNotification);
        label.setFont(juce::FontOptions(23.0f, juce::Font::bold));
        label.setColour(juce::Label::textColourId, juce::Colours::white);
    }

    void configureLabel(juce::Label& label, const char* text)
    {
        label.setText(zh(text), juce::dontSendNotification);
        label.setColour(juce::Label::textColourId, juce::Colour(0xffc8dbea));
    }

    void configureEditor(juce::TextEditor& editor, const char* placeholder, bool multiline = false)
    {
        editor.setMultiLine(multiline);
        editor.setReturnKeyStartsNewLine(multiline);
        editor.setTextToShowWhenEmpty(zh(placeholder), juce::Colour(0xff627a90));
        editor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff0c1d30));
        editor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        editor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff294861));
        editor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xff35cbed));
        editor.setFont(juce::FontOptions(15.0f));
    }

    void showGeneratePage()
    {
        generatePage.setVisible(true); recordsPage.setVisible(false);
        generateTab.setToggleState(true, juce::dontSendNotification);
        recordsTab.setToggleState(false, juce::dontSendNotification);
    }

    void showRecordsPage()
    {
        refreshRecordFilter();
        generatePage.setVisible(false); recordsPage.setVisible(true);
        generateTab.setToggleState(false, juce::dontSendNotification);
        recordsTab.setToggleState(true, juce::dontSendNotification);
    }

    void locateDefaultKey()
    {
        const auto support = store.dataFile().getParentDirectory().getChildFile("license-private-key.txt");
        const auto executable = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
        const juce::File candidates[] { support, executable.getParentDirectory().getChildFile("license-private-key.txt") };
        for (const auto& candidate : candidates)
            if (candidate.existsAsFile()) { privateKeyFile = candidate; break; }
        refreshKeyStatus();
    }

    void refreshKeyStatus()
    {
        const auto ready = privateKeyFile.existsAsFile();
        keyStatus.setText(ready ? zh("✓ 私钥已安全就绪\n请勿把本工具或私钥发给客户")
                                : zh("尚未找到私钥\n请选择 license-private-key.txt"), juce::dontSendNotification);
        keyStatus.setColour(juce::Label::textColourId,
                            ready ? juce::Colour(0xff61e6a7) : juce::Colour(0xffffbd72));
    }

    void choosePrivateKey()
    {
        chooser = std::make_unique<juce::FileChooser>(zh("选择风吟激活私钥"), juce::File(), "*.txt");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& selected)
            {
                const auto source = selected.getResult();
                if (! source.existsAsFile()) return;
                const juce::RSAKey testKey(source.loadFileAsString().trim());
                if (! testKey.isValid()) { showWarning("私钥无效", "请选择正确的 license-private-key.txt 文件。"); return; }
                const auto localKey = store.dataFile().getParentDirectory().getChildFile("license-private-key.txt");
                localKey.getParentDirectory().createDirectory();
                privateKeyFile = (source == localKey || source.copyFileTo(localKey)) ? localKey : source;
                refreshKeyStatus();
            });
    }

    void generateCode()
    {
        const auto machine = fengyin::ActivationRecordStore::formatMachineCode(machineCode.getText());
        if (! fengyin::ActivationRecordStore::isValidMachineCode(machine))
        { showWarning("机器码不正确", "机器码应包含 20 位字母或数字，通常显示为五组四位字符。"); return; }
        machineCode.setText(machine, false);
        if (! privateKeyFile.existsAsFile())
        { showWarning("尚未配置私钥", "请先点击右侧“选择私钥”，此操作只需完成一次。"); return; }
        if (const auto existing = store.findByMachineCode(machine))
        {
            activationCode.setText(existing->activationCode, false);
            juce::SystemClipboard::copyTextToClipboard(existing->activationCode);
            keyStatus.setText(zh("✓ 已调出历史激活码并复制\n同一台电脑不会重复生成记录"), juce::dontSendNotification);
            keyStatus.setColour(juce::Label::textColourId, juce::Colour(0xff61e6a7));
            return;
        }
        const juce::RSAKey key(privateKeyFile.loadFileAsString().trim());
        const auto licenseId = fengyin::ActivationRecordStore::createLicenseId();
        const auto code = fengyin::LicenseService::createActivationCode(machine, licenseId, key);
        if (code.isEmpty()) { showWarning("无法生成", "私钥无效，请重新选择正确的私钥文件。"); return; }
        fengyin::ActivationRecord record;
        record.id = juce::Uuid().toString();
        record.machineCode = machine;
        record.activationCode = code;
        record.licenseId = licenseId;
        record.customerName = customerName.getText();
        record.contact = contact.getText();
        record.orderSource = orderSource.getText();
        record.remark = remark.getText();
        record.createdAt = juce::Time::getCurrentTime().toISO8601(true);
        if (! store.add(record))
        { showWarning("保存失败", "激活码已经生成，但本地记录无法保存。请检查磁盘空间后重试。"); return; }
        activationCode.setText(code, false);
        juce::SystemClipboard::copyTextToClipboard(code);
        keyStatus.setText(zh("✓ 已生成、保存并复制\n现在可直接粘贴发给客户"), juce::dontSendNotification);
        keyStatus.setColour(juce::Label::textColourId, juce::Colour(0xff61e6a7));
        refreshRecordFilter();
    }

    void clearForm()
    {
        for (auto* editor : { &machineCode, &customerName, &contact, &orderSource, &remark, &activationCode }) editor->clear();
        refreshKeyStatus(); machineCode.grabKeyboardFocus();
    }

    void copyCurrentCode()
    {
        if (activationCode.getText().isNotEmpty()) juce::SystemClipboard::copyTextToClipboard(activationCode.getText());
    }

    void refreshRecordFilter()
    {
        filteredRows.clear();
        const auto query = search.getText().toLowerCase();
        const auto& all = store.records();
        for (int i = 0; i < static_cast<int>(all.size()); ++i)
        {
            const auto& item = all[static_cast<size_t>(i)];
            const auto searchable = item.createdAt + "\n" + item.customerName + "\n" + item.contact + "\n"
                + item.orderSource + "\n" + item.machineCode + "\n" + item.licenseId + "\n" + item.activationCode + "\n" + item.remark;
            if (query.isEmpty() || searchable.toLowerCase().contains(query)) filteredRows.push_back(i);
        }
        recordsCount.setText(zh("共 ") + juce::String(all.size()) + zh(" 条记录，本地自动保存"), juce::dontSendNotification);
        recordList.updateContent(); recordList.repaint();
    }

    int getNumRows() override { return static_cast<int>(filteredRows.size()); }

    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (row < 0 || row >= static_cast<int>(filteredRows.size())) return;
        const auto& item = store.records()[static_cast<size_t>(filteredRows[static_cast<size_t>(row)])];
        if (selected) { g.setColour(juce::Colour(0xff123a55)); g.fillRect(0, 0, width, height); }
        g.setColour(juce::Colour(0xff1f3a52)); g.drawHorizontalLine(height - 1, 12.0f, static_cast<float>(width - 12));
        g.setColour(juce::Colours::white); g.setFont(juce::FontOptions(15.5f, juce::Font::bold));
        g.drawText(item.customerName.isNotEmpty() ? item.customerName : zh("未填写客户昵称"), 16, 8, width - 32, 23,
                   juce::Justification::centredLeft, true);
        g.setColour(juce::Colour(0xff8da8bd)); g.setFont(juce::FontOptions(13.0f));
        g.drawText(item.machineCode + "    " + item.createdAt, 16, 36, width - 32, 22, juce::Justification::centredLeft, true);
    }

    void selectedRowsChanged(int selectedRow) override
    {
        if (selectedRow < 0 || selectedRow >= static_cast<int>(filteredRows.size())) return;
        selectedRecord = store.records()[static_cast<size_t>(filteredRows[static_cast<size_t>(selectedRow)])];
        const auto& item = *selectedRecord;
        const auto empty = zh("（未填写）");
        auto value = [&empty](const juce::String& text) { return text.isEmpty() ? empty : text; };
        recordDetails.setText(zh("客户昵称：") + value(item.customerName) + zh("\n联系方式：") + value(item.contact)
            + zh("\n订单来源：") + value(item.orderSource) + zh("\n生成时间：") + item.createdAt
            + zh("\n机器码：") + item.machineCode + zh("\n内部编号：") + item.licenseId
            + zh("\n\n激活码：\n") + item.activationCode + zh("\n\n备注：\n") + value(item.remark), false);
    }

    void exportRecords()
    {
        const auto defaultName = zh("风吟激活记录-") + juce::Time::getCurrentTime().formatted("%Y%m%d") + ".csv";
        exportChooser = std::make_unique<juce::FileChooser>(zh("导出激活记录备份"),
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile(defaultName), "*.csv");
        exportChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                 | juce::FileBrowserComponent::warnAboutOverwriting,
            [this](const juce::FileChooser& selected)
            {
                auto destination = selected.getResult();
                if (destination == juce::File()) return;
                if (! destination.hasFileExtension("csv")) destination = destination.withFileExtension("csv");
                if (! store.exportCsv(destination)) showWarning("导出失败", "无法写入所选位置，请换一个文件夹重试。");
            });
    }

    void showWarning(const char* titleText, const char* message)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, zh(titleText), zh(message));
    }

    ActivatorLookAndFeel lookAndFeel;
    fengyin::ActivationRecordStore store;
    juce::Component generatePage, recordsPage;
    juce::Label logo, title, subtitle, generateHeading, generateIntro, recordsHeading, recordsCount;
    juce::Label machineLabel, customerLabel, contactLabel, sourceLabel, remarkLabel, resultLabel, keyStatus;
    juce::TextEditor machineCode, customerName, contact, orderSource, remark, activationCode, search, recordDetails;
    juce::TextButton generateTab, recordsTab, generateButton, clearButton, copyButton, chooseKeyButton;
    juce::TextButton exportButton, copyRecordButton, openFolderButton;
    juce::ListBox recordList;
    juce::File privateKeyFile;
    std::vector<int> filteredRows;
    std::optional<fengyin::ActivationRecord> selectedRecord;
    std::unique_ptr<juce::FileChooser> chooser, exportChooser;
};

class ActivatorApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return zh("风吟激活码工具"); }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    void initialise(const juce::String&) override { window = std::make_unique<Window>(getApplicationName()); }
    void shutdown() override { window.reset(); }
private:
    class Window final : public juce::DocumentWindow
    {
    public:
        explicit Window(const juce::String& name) : DocumentWindow(name, juce::Colour(0xff07121f), DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true); setContentOwned(new ActivatorComponent(), true);
            setResizable(true, true); setResizeLimits(900, 620, 1600, 1100);
            centreWithSize(getWidth(), getHeight()); setVisible(true);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };
    std::unique_ptr<Window> window;
};
}

START_JUCE_APPLICATION(ActivatorApplication)

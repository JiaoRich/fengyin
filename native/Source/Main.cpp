#include <juce_gui_extra/juce_gui_extra.h>
#include "MainComponent.h"
#include "PluginScanWorker.h"

class FengYinApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return juce::String::fromUTF8("风吟"); }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override
    {
        return juce::JUCEApplication::getCommandLineParameters().startsWith("--scan-plugin ");
    }

    void initialise(const juce::String& commandLine) override
    {
        auto arguments = juce::StringArray::fromTokens(commandLine, true);
        arguments.removeEmptyStrings();
        if (arguments.size() == 3 && arguments[0] == "--scan-plugin")
        {
            scanWorker = std::make_unique<PluginScanWorker>(arguments[1].unquoted(), juce::File(arguments[2].unquoted()));
            return;
        }
        mainWindow = std::make_unique<MainWindow>(getApplicationName());
    }

    void shutdown() override { scanWorker.reset(); mainWindow.reset(); }
    void systemRequestedQuit() override { quit(); }

private:
    class MainWindow final : public juce::DocumentWindow
    {
    public:
        explicit MainWindow(juce::String name)
            : DocumentWindow(std::move(name), juce::Colour(0xff07101d), allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(), true);
            setResizable(true, true);
            setResizeLimits(1120, 700, 2560, 1600);
            const auto display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
            const auto available = display != nullptr ? display->userBounds.toNearestInt()
                                                      : juce::Rectangle<int>(0, 0, 1366, 768);
            centreWithSize(juce::jmin(1440, juce::jmax(1120, available.getWidth() - 32)),
                           juce::jmin(900, juce::jmax(700, available.getHeight() - 32)));
            setVisible(true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

    std::unique_ptr<MainWindow> mainWindow;
    std::unique_ptr<PluginScanWorker> scanWorker;
};

START_JUCE_APPLICATION(FengYinApplication)

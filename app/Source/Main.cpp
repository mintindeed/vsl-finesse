#include <juce_gui_extra/juce_gui_extra.h>

#include "AppContext.h"
#include "MainComponent.h"
#include "Theme.h"

class VSLFinesseApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override          { return false; }

    void initialise (const juce::String& commandLine) override
    {
        auto& lnf = juce::LookAndFeel::getDefaultLookAndFeel();
        lnf.setColour (juce::ResizableWindow::backgroundColourId, theme::background);

        context = std::make_unique<AppContext>();
        window = std::make_unique<MainWindow> (getApplicationName(), *context);
        openFromCommandLine (commandLine);
    }

    void shutdown() override
    {
        window.reset();
        context.reset();
    }

    void systemRequestedQuit() override { quit(); }

    // Finder "Open With" / dropping files on the Dock icon.
    void anotherInstanceStarted (const juce::String& commandLine) override { openFromCommandLine (commandLine); }

private:
    void openFromCommandLine (const juce::String& commandLine)
    {
        juce::StringArray paths;
        for (auto token : juce::StringArray::fromTokens (commandLine, true))
        {
            token = token.unquoted();
            if (juce::File::isAbsolutePath (token) && juce::File (token).existsAsFile())
                paths.add (token);
        }
        if (! paths.isEmpty() && window != nullptr)
            window->content->openFiles (paths);
    }

    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow (const juce::String& name, AppContext& ctx)
            : DocumentWindow (name, theme::background, DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            content = new MainComponent (ctx);
            setContentOwned (content, true);
           #if ! JUCE_MAC
            setMenuBar (content);
           #endif
            setResizable (true, false);
            setResizeLimits (960, 600, 10000, 10000);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }

        ~MainWindow() override
        {
           #if ! JUCE_MAC
            setMenuBar (nullptr);
           #endif
        }

        void closeButtonPressed() override { JUCEApplication::getInstance()->systemRequestedQuit(); }

        MainComponent* content = nullptr;
    };

    std::unique_ptr<AppContext> context;
    std::unique_ptr<MainWindow> window;
};

START_JUCE_APPLICATION (VSLFinesseApplication)

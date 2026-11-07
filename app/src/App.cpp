#include "vyra/audio/AudioEngine.h"
#include "vyra/integrations/DiscordPresence.h"
#include "vyra/library/TrackLibrary.h"
#include "vyra/ui/MainComponent.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace
{
class SplashContent final : public juce::Component,
                            private juce::Timer
{
public:
    SplashContent()
    {
        startTimerHz(30);
    }

    void setLoadingState(float newProgress, juce::String newStatus)
    {
        progress = std::clamp(newProgress, 0.0f, 1.0f);
        status = std::move(newStatus);
        failed = false;
        repaint();
    }

    void setFailure(juce::String message, std::function<void()> quit)
    {
        status = "Startup failed: " + message;
        onQuit = std::move(quit);
        failed = true;
        repaint();
    }

    void paint(juce::Graphics& graphics) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto card = bounds.reduced(0.5f);
        graphics.fillAll(juce::Colour(0xff242627));
        graphics.setColour(juce::Colour(0x183b4242));
        graphics.drawRect(card, 0.75f);
        drawAccent(graphics, card);

        graphics.setColour(juce::Colour(0xfff0f1f0));
        graphics.setFont(juce::Font(48.0f, juce::Font::plain));
        graphics.drawText("Vyra", card.withY(card.getY() + 132.0f)
                              .withHeight(58.0f),
                          juce::Justification::centred, false);
        graphics.setColour(juce::Colour(0xffbbc2bf));
        graphics.setFont(juce::Font(10.0f));
        graphics.drawText("v0.2.0", card.withY(card.getY() + 196.0f)
                               .withHeight(18.0f),
                          juce::Justification::centred, false);

        const auto left = card.getX() + 5.0f;
        const auto barY = card.getBottom() - 8.0f;
        const auto barWidth = card.getWidth() - 10.0f;
        if (failed)
        {
            graphics.setColour(juce::Colour(0xfff0f1f0));
            graphics.setFont(juce::Font(10.0f));
            graphics.drawText(status, static_cast<int>(left),
                              static_cast<int>(barY - 28.0f),
                              static_cast<int>(barWidth - 68.0f), 18,
                              juce::Justification::centredLeft, true);
            graphics.setColour(juce::Colour(0xff46514e));
            graphics.fillRect(static_cast<int>(card.getRight() - 68.0f),
                              static_cast<int>(barY - 32.0f), 60, 24);
            graphics.setColour(juce::Colours::white);
            graphics.drawText("QUIT", static_cast<int>(card.getRight() - 68.0f),
                              static_cast<int>(barY - 32.0f), 60, 24,
                              juce::Justification::centred, false);
            quitBounds = { static_cast<int>(card.getRight() - 68.0f),
                           static_cast<int>(barY - 32.0f), 60, 24 };
        }
        else
        {
            graphics.setColour(juce::Colour(0xfff0f1f0));
            graphics.setFont(juce::Font(10.0f));
            graphics.drawText(status, static_cast<int>(left),
                              static_cast<int>(barY - 18.0f),
                              static_cast<int>(barWidth), 14,
                              juce::Justification::centredLeft, true);
        }

        graphics.setColour(juce::Colour(0xff373b3c));
        graphics.fillRoundedRectangle(left, barY, barWidth, 6.0f, 3.0f);
        graphics.setColour(juce::Colour(0xff00c36a));
        graphics.fillRoundedRectangle(left, barY,
                                      barWidth * progress, 6.0f, 3.0f);
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        if (failed && quitBounds.contains(event.getPosition()) && onQuit)
            onQuit();
    }

private:
    void timerCallback() override
    {
        if (!failed)
        {
            ribbonPhase += 0.018f;
            if (ribbonPhase > juce::MathConstants<float>::twoPi)
                ribbonPhase -= juce::MathConstants<float>::twoPi;
            repaint();
        }
    }

    void drawAccent(juce::Graphics& graphics, juce::Rectangle<float> card)
    {
        const auto centreX = card.getCentreX();
        const auto centreY = card.getY() + 172.0f;
        const auto phase = std::sin(ribbonPhase) * 3.0f;
        juce::Path ribbon;
        ribbon.startNewSubPath(centreX - 185.0f, centreY + 10.0f + phase);
        ribbon.cubicTo(centreX - 118.0f, centreY - 43.0f,
                       centreX - 64.0f, centreY + 44.0f,
                       centreX, centreY - 3.0f);
        ribbon.cubicTo(centreX + 65.0f, centreY - 48.0f,
                       centreX + 114.0f, centreY + 34.0f,
                       centreX + 185.0f, centreY - 8.0f - phase);

        graphics.setColour(juce::Colour(0xff00c36a).withAlpha(0.035f));
        graphics.strokePath(ribbon, juce::PathStrokeType(
            26.0f, juce::PathStrokeType::curved,
            juce::PathStrokeType::rounded));
        graphics.setColour(juce::Colour(0xff00c36a).withAlpha(0.075f));
        graphics.strokePath(ribbon, juce::PathStrokeType(
            9.0f, juce::PathStrokeType::curved,
            juce::PathStrokeType::rounded));
        graphics.setColour(juce::Colour(0xff20dfa0).withAlpha(0.12f));
        graphics.strokePath(ribbon, juce::PathStrokeType(
            1.2f, juce::PathStrokeType::curved,
            juce::PathStrokeType::rounded));

        for (int line = 0; line < 3; ++line)
        {
            const auto offset = static_cast<float>(line - 1) * 12.0f;
            juce::Path contour;
            contour.startNewSubPath(centreX - 168.0f, centreY + offset);
            contour.cubicTo(centreX - 96.0f, centreY - 24.0f + offset,
                            centreX - 42.0f, centreY + 24.0f + offset,
                            centreX + 6.0f, centreY + 2.0f + offset);
            contour.cubicTo(centreX + 75.0f, centreY - 28.0f + offset,
                            centreX + 112.0f, centreY + 23.0f + offset,
                            centreX + 168.0f, centreY - 3.0f + offset);
            graphics.setColour(juce::Colour(0xff4c8f7a).withAlpha(
                line == 1 ? 0.10f : 0.055f));
            graphics.strokePath(contour, juce::PathStrokeType(
                0.8f, juce::PathStrokeType::curved,
                juce::PathStrokeType::rounded));
        }
    }

    float progress = 0.0f;
    float ribbonPhase = 0.0f;
    juce::String status { "Starting Vyra..." };
    juce::Rectangle<int> quitBounds;
    std::function<void()> onQuit;
    bool failed = false;
};

class SplashWindow final : public juce::DocumentWindow
{
public:
    SplashWindow()
        : juce::DocumentWindow("Vyra", juce::Colour(0xff242627),
                               juce::DocumentWindow::allButtons, true)
    {
        setUsingNativeTitleBar(false);
        setTitleBarHeight(0);
        setContentOwned(new SplashContent(), true);
        setResizable(false, false);
        centreWithSize(500, 330);
        setVisible(true);
    }

    void setLoadingState(float progress, juce::String status)
    {
        if (auto* content = dynamic_cast<SplashContent*>(getContentComponent()))
            content->setLoadingState(progress, std::move(status));
    }

    void setFailure(juce::String message, std::function<void()> quit)
    {
        if (auto* content = dynamic_cast<SplashContent*>(getContentComponent()))
            content->setFailure(std::move(message), std::move(quit));
    }
};

class WindowControlButton final : public juce::Button
{
public:
    explicit WindowControlButton(int type)
        : juce::Button("Vyra window control"), buttonType(type)
    {
        setWantsKeyboardFocus(false);
    }

    void paintButton(juce::Graphics& graphics, bool highlighted, bool down) override
    {
        const auto bounds = getLocalBounds().toFloat();
        if (down || highlighted)
        {
            const auto background = buttonType == juce::DocumentWindow::closeButton
                ? juce::Colour(0xffc43d45) : juce::Colour(0xff292f32);
            graphics.setColour(background);
            graphics.fillRect(bounds);
        }

        graphics.setColour(juce::Colour(0xffc8cecb));
        const auto centreX = bounds.getCentreX();
        const auto centreY = bounds.getCentreY();
        if (buttonType == juce::DocumentWindow::minimiseButton)
        {
            graphics.drawLine(centreX - 4.0f, centreY + 2.0f,
                              centreX + 4.0f, centreY + 2.0f, 1.2f);
        }
        else if (buttonType == juce::DocumentWindow::maximiseButton)
        {
            if (getToggleState())
            {
                graphics.drawRect(centreX - 3.0f, centreY - 4.0f,
                                  6.0f, 6.0f, 1.0f);
                graphics.drawLine(centreX - 5.0f, centreY + 3.0f,
                                  centreX + 2.0f, centreY + 3.0f, 1.0f);
            }
            else
            {
                graphics.drawRect(centreX - 4.0f, centreY - 4.0f,
                                  8.0f, 8.0f, 1.0f);
            }
        }
        else
        {
            graphics.drawLine(centreX - 4.0f, centreY - 4.0f,
                              centreX + 4.0f, centreY + 4.0f, 1.2f);
            graphics.drawLine(centreX + 4.0f, centreY - 4.0f,
                              centreX - 4.0f, centreY + 4.0f, 1.2f);
        }
    }

private:
    const int buttonType;
};

class VyraTitleBarLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    VyraTitleBarLookAndFeel()
    {
        setColourScheme(getDarkColourScheme());
    }

    juce::Button* createDocumentWindowButton(int buttonType) override
    {
        return new WindowControlButton(buttonType);
    }

    void drawDocumentWindowTitleBar(juce::DocumentWindow& window,
                                    juce::Graphics& graphics,
                                    int width, int height,
                                    int titleSpaceX, int titleSpaceWidth,
                                    const juce::Image*, bool) override
    {
        graphics.fillAll(juce::Colour(0xff111416));
        graphics.setColour(juce::Colour(0xff2b3032));
        graphics.drawLine(0.0f, static_cast<float>(height) - 0.5f,
                          static_cast<float>(width),
                          static_cast<float>(height) - 0.5f, 1.0f);

        const auto markX = static_cast<float>(titleSpaceX + 8);
        const auto markY = static_cast<float>(height) * 0.5f;
        juce::Path mark;
        mark.startNewSubPath(markX, markY - 6.0f);
        mark.lineTo(markX + 4.0f, markY + 6.0f);
        mark.lineTo(markX + 8.0f, markY - 6.0f);
        graphics.setColour(juce::Colour(0xff00c36a));
        graphics.strokePath(mark, juce::PathStrokeType(
            2.2f, juce::PathStrokeType::curved,
            juce::PathStrokeType::rounded));

        graphics.setColour(juce::Colour(0xffe6e9e8));
        graphics.setFont(juce::Font(13.0f, juce::Font::bold));
        graphics.drawText(window.getName(),
                          titleSpaceX + 25, 0,
                          juce::jmax(1, titleSpaceWidth - 25), height,
                          juce::Justification::centredLeft, false);
    }
};

class VyraWindow final : public juce::DocumentWindow
{
public:
    VyraWindow(vyra::audio::AudioEngine& engine,
               vyra::library::TrackLibrary& library,
               vyra::integrations::DiscordPresence& discordPresence)
        : juce::DocumentWindow("Vyra", juce::Colour(0xff101317),
                               juce::DocumentWindow::allButtons, true)
    {
        setLookAndFeel(&titleBarLookAndFeel);
        setTitleBarButtonsRequired(juce::DocumentWindow::allButtons, false);
        setUsingNativeTitleBar(false);
        setTitleBarHeight(34);
        setTitleBarTextCentred(false);
        setContentOwned(new vyra::ui::MainComponent(engine, library, discordPresence), true);
        setResizable(true, true);
        setResizeLimits(800, 560, 2560, 1600);
        centreWithSize(1280, 760);
        setVisible(true);
    }

    ~VyraWindow() override
    {
        setLookAndFeel(nullptr);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
    VyraTitleBarLookAndFeel titleBarLookAndFeel;
};

class VyraApplication final : public juce::JUCEApplication,
                              private juce::Timer
{
public:
    const juce::String getApplicationName() override { return "Vyra"; }
    const juce::String getApplicationVersion() override { return "0.2.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise(const juce::String&) override
    {
#if JUCE_WINDOWS
        juce::LookAndFeel::getDefaultLookAndFeel()
            .setDefaultSansSerifTypefaceName("Segoe UI");
#endif
        splashStartedAt = juce::Time::getMillisecondCounter();
        splashWindow = std::make_unique<SplashWindow>();
        startTimerHz(30);
        initializationThread = std::thread([this]
        {
            try
            {
                setLoadingState(0.08f, "Initializing audio engine...");
                pendingEngine = std::make_unique<vyra::audio::AudioEngine>();

                setLoadingState(0.55f, "Opening music library...");
                const auto libraryDirectory = juce::File::getSpecialLocation(
                    juce::File::userApplicationDataDirectory).getChildFile("Vyra");
                pendingLibrary = std::make_unique<vyra::library::TrackLibrary>(
                    libraryDirectory.getChildFile("library.sqlite3"));

                setLoadingState(0.86f, "Audio and library are ready.");
            }
            catch (const std::exception& exception)
            {
                {
                    std::lock_guard lock(startupMutex);
                    startupError = exception.what();
                }
            }
            initializationFinished.store(true, std::memory_order_release);
        });
    }

    void shutdown() override
    {
        if (initializationThread.joinable())
            initializationThread.join();
        window.reset();
        discordPresence.reset();
        trackLibrary.reset();
        engine.reset();
        pendingLibrary.reset();
        pendingEngine.reset();
        splashWindow.reset();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

private:
    void setLoadingState(float progress, juce::String status)
    {
        {
            std::lock_guard lock(startupMutex);
            startupStatus = status;
        }
        startupProgress.store(progress, std::memory_order_relaxed);
    }

    void timerCallback() override
    {
        juce::String status;
        {
            std::lock_guard lock(startupMutex);
            status = startupStatus;
        }
        if (splashWindow != nullptr)
            splashWindow->setLoadingState(
                startupProgress.load(std::memory_order_relaxed), std::move(status));

        if (!initializationFinished.load(std::memory_order_acquire))
            return;

        if (initializationThread.joinable())
            initializationThread.join();

        juce::String error;
        {
            std::lock_guard lock(startupMutex);
            error = startupError;
        }
        if (error.isNotEmpty())
        {
            stopTimer();
            if (splashWindow != nullptr)
                splashWindow->setFailure(std::move(error), [this] { quit(); });
            return;
        }

        if (juce::Time::getMillisecondCounter() - splashStartedAt
            < minimumSplashDurationMs)
            return;

        stopTimer();
        try
        {
            engine = std::move(pendingEngine);
            trackLibrary = std::move(pendingLibrary);
            splashWindow->setLoadingState(0.94f, "Connecting optional integrations...");
            discordPresence = std::make_unique<vyra::integrations::DiscordPresence>();
            window = std::make_unique<VyraWindow>(
                *engine, *trackLibrary, *discordPresence);
            splashWindow.reset();
        }
        catch (const std::exception& exception)
        {
            if (splashWindow != nullptr)
                splashWindow->setFailure(exception.what(), [this] { quit(); });
        }
    }

    std::unique_ptr<vyra::audio::AudioEngine> pendingEngine;
    std::unique_ptr<vyra::library::TrackLibrary> pendingLibrary;
    std::unique_ptr<vyra::audio::AudioEngine> engine;
    std::unique_ptr<vyra::library::TrackLibrary> trackLibrary;
    std::unique_ptr<vyra::integrations::DiscordPresence> discordPresence;
    std::unique_ptr<SplashWindow> splashWindow;
    std::unique_ptr<VyraWindow> window;
    std::thread initializationThread;
    juce::uint32 splashStartedAt = 0;
    static constexpr juce::uint32 minimumSplashDurationMs = 2500;
    std::atomic<float> startupProgress { 0.0f };
    std::atomic<bool> initializationFinished { false };
    std::mutex startupMutex;
    juce::String startupStatus { "Starting Vyra..." };
    juce::String startupError;
};
}

START_JUCE_APPLICATION(VyraApplication)

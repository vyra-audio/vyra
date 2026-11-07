#pragma once

#include "vyra/audio/AudioEngine.h"
#include "vyra/integrations/DiscordPresence.h"
#include "vyra/integrations/MidiController.h"
#include "vyra/library/TrackLibrary.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace vyra::ui
{
class VyraLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawButtonBackground(juce::Graphics& graphics, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool highlighted, bool down) override;
    void drawLinearSlider(juce::Graphics& graphics, int x, int y, int width,
                          int height, float sliderPosition, float minSliderPosition,
                          float maxSliderPosition, juce::Slider::SliderStyle style,
                          juce::Slider& slider) override;
    void drawRotarySlider(juce::Graphics& graphics, int x, int y, int width,
                          int height, float sliderPosition, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override;
};

class LevelMeter final : public juce::Component
{
public:
    void setLevel(float newLevel) noexcept;
    void paint(juce::Graphics& graphics) override;

private:
    std::atomic<float> level { 0.0f };
};

class DeckComponent final : public juce::Component,
                            private juce::Timer
{
public:
    DeckComponent(audio::AudioEngine& engine, library::TrackLibrary& library,
                  std::size_t deckIndex);
    ~DeckComponent() override;

    void loadFile(juce::File file, std::vector<std::uint8_t> cachedWaveform = {},
                  juce::String displayTitle = {});
    [[nodiscard]] juce::String getTrackTitle() const;
    [[nodiscard]] juce::String getTrackPath() const;
    [[nodiscard]] std::uint64_t getLoadRevision() const noexcept;
    [[nodiscard]] bool hasLoadedTrack() const;
    void triggerLoad();
    void seekToNormalized(float position);
    void triggerPlay();
    void triggerCue();
    void triggerHotCue(std::size_t cueIndex);
    void triggerLoop();
    void triggerBeatJump(bool forward);
    void triggerSync();
    void triggerHeadphoneCue();
    void triggerJog(float relativeSteps);
    void setGainFromController(float value);
    void setPitchFromController(float value);
    void setEqFromController(std::size_t band, float value);
    void setFilterFromController(float value);
    void setEchoFromController(float value);
    void paintMiniOverview(juce::Graphics& graphics,
                           juce::Rectangle<float> bounds) const;
    void paint(juce::Graphics& graphics) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;

private:
    void timerCallback() override;
    void openTrackChooser();
    void refreshHotCueButton(std::size_t cueIndex);
    void savePerformanceState();

    audio::AudioEngine& engine;
    library::TrackLibrary& library;
    const std::size_t deckIndex;
    juce::Label deckTitle;
    juce::Label trackTitle;
    juce::Label timeLabel;
    juce::TextButton loadButton { "LOAD TRACK" };
    juce::TextButton playButton { "PLAY" };
    juce::TextButton cueButton { "CUE" };
    juce::TextButton pflButton { "PFL" };
    std::array<juce::TextButton, 3> hotCueButtons;
    std::array<juce::TextButton, 2> beatJumpButtons;
    juce::TextButton loopButton { "LOOP" };
    juce::ComboBox loopBeatsBox;
    juce::TextButton syncButton { "SYNC" };
    juce::Slider pitchSlider;
    juce::Label pitchLabel { {}, "PITCH" };
    std::array<std::optional<double>, 3> hotCuePositions;
    juce::Slider positionSlider;
    juce::Slider gainSlider;
    juce::Label gainLabel { {}, "CHANNEL" };
    std::array<juce::Slider, 3> eqSliders;
    std::array<juce::Label, 3> eqLabels;
    juce::Slider filterSlider;
    juce::Label filterLabel { {}, "FILTER" };
    juce::Slider echoSlider;
    juce::Label echoLabel { {}, "ECHO" };
    LevelMeter meter;
    std::unique_ptr<juce::FileChooser> fileChooser;
    std::vector<std::uint8_t> waveform;
    juce::String trackPath;
    std::uint64_t loadRevision = 0;
    bool updatingPosition = false;
    double sliderLength = -1.0;
};

class LibraryWindow final : public juce::DocumentWindow
{
public:
    LibraryWindow(library::TrackLibrary& library,
                  std::function<void(std::size_t, library::Track)> loadTrack);
    void closeButtonPressed() override;
};

class MainComponent final : public juce::Component,
                            private juce::Timer
{
public:
    MainComponent(audio::AudioEngine& engine, library::TrackLibrary& library,
                  integrations::DiscordPresence& discordPresence);
    ~MainComponent() override;

    void paint(juce::Graphics& graphics) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

private:
    void timerCallback() override;
    void openAudioSettings();
    void openLibrary();
    void seekOverviewTo(int deckIndex, int x);

    audio::AudioEngine& engine;
    library::TrackLibrary& library;
    integrations::DiscordPresence& discordPresence;
    integrations::MidiController midiController;
    VyraLookAndFeel lookAndFeel;
    juce::Label wordmark;
    juce::Label statusLabel;
    juce::TextButton audioSettingsButton { "SETTINGS" };
    juce::TextButton recordButton { "REC WAV" };
    juce::TextButton pauseRecordingButton { "PAUSE" };
    DeckComponent leftDeck;
    DeckComponent rightDeck;
    juce::Label crossfaderLabel { {}, "CROSSFADER" };
    juce::Slider crossfader;
    juce::Label masterLabel { {}, "MASTER" };
    juce::Slider masterGain;
    LevelMeter masterMeter;
    juce::TextButton libraryButton { "LIBRARY" };
    std::unique_ptr<juce::Component> libraryPanel;
    std::unique_ptr<juce::FileChooser> recordingChooser;
    std::unique_ptr<LibraryWindow> libraryWindow;
    std::size_t lastPresenceDeck = 0;
    int overviewDeckBeingDragged = -1;
    bool hasPresenceTrack = false;
    std::array<std::uint64_t, 2> recordedLoadRevisions {};
};
}

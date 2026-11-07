#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_data_structures/juce_data_structures.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace vyra::integrations
{
class MidiController final : private juce::MidiInputCallback,
                             private juce::Timer
{
public:
    enum class Action : int
    {
        playDeck1,
        cueDeck1,
        hotCue1Deck1,
        hotCue2Deck1,
        hotCue3Deck1,
        playDeck2,
        cueDeck2,
        hotCue1Deck2,
        hotCue2Deck2,
        hotCue3Deck2,
        crossfader,
        gainDeck1,
        gainDeck2,
        loopDeck1,
        loopDeck2,
        beatJumpBackDeck1,
        beatJumpForwardDeck1,
        beatJumpBackDeck2,
        beatJumpForwardDeck2,
        syncDeck1,
        syncDeck2,
        headphoneCueDeck1,
        headphoneCueDeck2,
        pitchDeck1,
        pitchDeck2,
        lowEqDeck1,
        midEqDeck1,
        highEqDeck1,
        lowEqDeck2,
        midEqDeck2,
        highEqDeck2,
        jogDeck1,
        jogDeck2,
        filterDeck1,
        filterDeck2,
        echoDeck1,
        echoDeck2,
        masterGain,
        count
    };

    enum class MessageType : int
    {
        note,
        controller,
        pitchWheel
    };

    struct Binding
    {
        Action action;
        MessageType type;
        int channel;
        int number;
    };

    using BindingSet = std::array<std::optional<Binding>,
                                  static_cast<std::size_t>(Action::count)>;

    struct Profile
    {
        juce::String name;
        std::vector<Binding> bindings;
    };

    using ActionHandler = std::function<void(Action, float)>;

    MidiController();
    ~MidiController() override;

    MidiController(const MidiController&) = delete;
    MidiController& operator=(const MidiController&) = delete;

    [[nodiscard]] static const std::vector<Profile>& getProfiles();
    [[nodiscard]] static const std::array<Action, static_cast<std::size_t>(Action::count)>&
        getActions();
    [[nodiscard]] static juce::String getActionName(Action action);
    [[nodiscard]] static std::optional<Binding> getBindingFromProfile(
        int profileIndex, Action action);
    [[nodiscard]] static bool matches(const Binding& binding,
                                      const juce::MidiMessage& message) noexcept;
    [[nodiscard]] static float pitchPercentFromMidi(float normalizedValue) noexcept;
    [[nodiscard]] static float normalizedPitchValueFromMidi14Bit(
        int midiValue) noexcept;
    [[nodiscard]] static float relativeJogDeltaFromMidi(int midiValue) noexcept;
    [[nodiscard]] static bool exportMappingFile(const juce::File& file,
                                                const BindingSet& bindings,
                                                juce::String& error);
    [[nodiscard]] static std::optional<BindingSet> importMappingFile(
        const juce::File& file, juce::String& error);

    [[nodiscard]] static juce::Array<juce::MidiDeviceInfo> getAvailableDevices();
    [[nodiscard]] bool selectDevice(const juce::String& identifier);
    [[nodiscard]] juce::String getSelectedDeviceIdentifier() const;
    [[nodiscard]] juce::String getStatus() const;

    void setActionHandler(ActionHandler handler);
    void applyProfile(int profileIndex);
    void beginLearn(Action action);
    void cancelLearn();
    [[nodiscard]] bool isLearning() const noexcept;
    [[nodiscard]] std::optional<Binding> getBinding(Action action) const;
    [[nodiscard]] juce::String getBindingDescription(Action action) const;
    [[nodiscard]] bool exportBindings(const juce::File& file);
    [[nodiscard]] bool importBindings(const juce::File& file);

private:
    struct MidiEvent
    {
        MessageType type = MessageType::note;
        int channel = 1;
        int number = 0;
        int value = 0;
        bool pressed = false;
    };

    void handleIncomingMidiMessage(juce::MidiInput* source,
                                   const juce::MidiMessage& message) override;
    void timerCallback() override;
    void savePreferences();
    void loadBindings();
    void setStatus(juce::String message);
    [[nodiscard]] static bool isContinuous(Action action) noexcept;
    [[nodiscard]] static std::optional<MidiEvent> makeEvent(
        const juce::MidiMessage& message) noexcept;

    static constexpr std::size_t actionCount = static_cast<std::size_t>(Action::count);
    static constexpr int queueCapacity = 256;
    juce::AbstractFifo eventFifo { queueCapacity };
    std::array<MidiEvent, queueCapacity> eventQueue {};
    BindingSet bindings {};
    std::unique_ptr<juce::PropertiesFile> preferences;
    std::unique_ptr<juce::MidiInput> midiInput;
    ActionHandler actionHandler;
    std::atomic<int> learningAction { -1 };
    std::atomic<int> droppedEvents { 0 };
    juce::String selectedDeviceIdentifier;
    juce::String status { "MIDI input disconnected" };
};
}

#include "vyra/integrations/MidiController.h"

#include <algorithm>

namespace vyra::integrations
{
namespace
{
using Action = MidiController::Action;
using Binding = MidiController::Binding;
using MessageType = MidiController::MessageType;

Binding note(Action action, int channel, int number)
{
    return { action, MessageType::note, channel, number };
}

Binding controller(Action action, int channel, int number)
{
    return { action, MessageType::controller, channel, number };
}

const std::vector<MidiController::Profile>& profiles()
{
    static const std::vector<MidiController::Profile> values {
        { "Generic MIDI (learn)", {} },
        { "Numark Party Mix II (MIDI Learn)", {} },
        { "Numark Party Mix (original generation)",
          { note(Action::playDeck1, 1, 0), note(Action::cueDeck1, 1, 1),
            note(Action::playDeck2, 2, 0), note(Action::cueDeck2, 2, 1),
            controller(Action::crossfader, 16, 8) } },
        { "Pioneer DDJ-400",
          { note(Action::playDeck1, 1, 0x0b), note(Action::cueDeck1, 1, 0x0c),
            note(Action::playDeck2, 2, 0x0b), note(Action::cueDeck2, 2, 0x0c) } },
        { "Pioneer DDJ-FLX4",
          { note(Action::playDeck1, 1, 0x0b), note(Action::cueDeck1, 1, 0x0c),
            note(Action::playDeck2, 2, 0x0b), note(Action::cueDeck2, 2, 0x0c) } },
        { "Hercules DJControl Inpulse 500",
          { note(Action::playDeck1, 2, 0x07), note(Action::cueDeck1, 2, 0x06),
            note(Action::playDeck2, 3, 0x07), note(Action::cueDeck2, 3, 0x06) } }
    };
    return values;
}
}

MidiController::MidiController()
{
    juce::PropertiesFile::Options options;
    options.applicationName = "Vyra";
    options.filenameSuffix = "settings";
    options.folderName = "Vyra";
    options.osxLibrarySubFolder = "Application Support";
    preferences = std::make_unique<juce::PropertiesFile>(options);

    selectedDeviceIdentifier = preferences->getValue("midi.device");
    loadBindings();
    startTimerHz(60);

    if (selectedDeviceIdentifier.isNotEmpty())
    {
        const auto opened = selectDevice(selectedDeviceIdentifier);
        juce::ignoreUnused(opened);
    }
}

MidiController::~MidiController()
{
    stopTimer();
    if (midiInput != nullptr)
    {
        midiInput->stop();
        midiInput.reset();
    }
}

const std::vector<MidiController::Profile>& MidiController::getProfiles()
{
    return profiles();
}

const std::array<MidiController::Action,
                 static_cast<std::size_t>(MidiController::Action::count)>&
MidiController::getActions()
{
    static constexpr std::array<Action, actionCount> actions {
        Action::playDeck1, Action::cueDeck1, Action::hotCue1Deck1,
        Action::hotCue2Deck1, Action::hotCue3Deck1, Action::playDeck2,
        Action::cueDeck2, Action::hotCue1Deck2, Action::hotCue2Deck2,
        Action::hotCue3Deck2, Action::crossfader, Action::gainDeck1,
        Action::gainDeck2, Action::loopDeck1, Action::loopDeck2,
        Action::beatJumpBackDeck1, Action::beatJumpForwardDeck1,
        Action::beatJumpBackDeck2, Action::beatJumpForwardDeck2,
        Action::syncDeck1, Action::syncDeck2, Action::headphoneCueDeck1,
        Action::headphoneCueDeck2, Action::pitchDeck1, Action::pitchDeck2,
        Action::lowEqDeck1, Action::midEqDeck1, Action::highEqDeck1,
        Action::lowEqDeck2, Action::midEqDeck2, Action::highEqDeck2,
        Action::filterDeck1, Action::filterDeck2, Action::echoDeck1,
        Action::echoDeck2, Action::masterGain, Action::jogDeck1,
        Action::jogDeck2
    };
    return actions;
}

juce::String MidiController::getActionName(Action action)
{
    switch (action)
    {
        case Action::playDeck1: return "Deck 1 · Play/Pause";
        case Action::cueDeck1: return "Deck 1 · Cue";
        case Action::hotCue1Deck1: return "Deck 1 · Hot Cue 1";
        case Action::hotCue2Deck1: return "Deck 1 · Hot Cue 2";
        case Action::hotCue3Deck1: return "Deck 1 · Hot Cue 3";
        case Action::playDeck2: return "Deck 2 · Play/Pause";
        case Action::cueDeck2: return "Deck 2 · Cue";
        case Action::hotCue1Deck2: return "Deck 2 · Hot Cue 1";
        case Action::hotCue2Deck2: return "Deck 2 · Hot Cue 2";
        case Action::hotCue3Deck2: return "Deck 2 · Hot Cue 3";
        case Action::crossfader: return "Mixer · Crossfader";
        case Action::gainDeck1: return "Deck 1 · Channel level";
        case Action::gainDeck2: return "Deck 2 · Channel level";
        case Action::loopDeck1: return "Deck 1 · Loop toggle";
        case Action::loopDeck2: return "Deck 2 · Loop toggle";
        case Action::beatJumpBackDeck1: return "Deck 1 · Beat jump back";
        case Action::beatJumpForwardDeck1: return "Deck 1 · Beat jump forward";
        case Action::beatJumpBackDeck2: return "Deck 2 · Beat jump back";
        case Action::beatJumpForwardDeck2: return "Deck 2 · Beat jump forward";
        case Action::syncDeck1: return "Deck 1 · Sync";
        case Action::syncDeck2: return "Deck 2 · Sync";
        case Action::headphoneCueDeck1: return "Deck 1 · Headphone cue";
        case Action::headphoneCueDeck2: return "Deck 2 · Headphone cue";
        case Action::pitchDeck1: return "Deck 1 · Pitch";
        case Action::pitchDeck2: return "Deck 2 · Pitch";
        case Action::lowEqDeck1: return "Deck 1 · Low EQ";
        case Action::midEqDeck1: return "Deck 1 · Mid EQ";
        case Action::highEqDeck1: return "Deck 1 · High EQ";
        case Action::lowEqDeck2: return "Deck 2 · Low EQ";
        case Action::midEqDeck2: return "Deck 2 · Mid EQ";
        case Action::highEqDeck2: return "Deck 2 · High EQ";
        case Action::filterDeck1: return "Deck 1 · Filter";
        case Action::filterDeck2: return "Deck 2 · Filter";
        case Action::echoDeck1: return "Deck 1 · Echo amount";
        case Action::echoDeck2: return "Deck 2 · Echo amount";
        case Action::masterGain: return "Mixer · Master level";
        case Action::jogDeck1: return "Deck 1 · Relative jog";
        case Action::jogDeck2: return "Deck 2 · Relative jog";
        case Action::count: break;
    }
    return "Unknown action";
}

std::optional<MidiController::Binding> MidiController::getBindingFromProfile(
    int profileIndex, Action action)
{
    if (profileIndex < 0 || profileIndex >= static_cast<int>(profiles().size()))
        return std::nullopt;

    const auto& profile = profiles()[static_cast<std::size_t>(profileIndex)];
    const auto binding = std::find_if(profile.bindings.begin(), profile.bindings.end(),
        [action](const Binding& candidate) { return candidate.action == action; });
    if (binding == profile.bindings.end())
        return std::nullopt;
    return *binding;
}

bool MidiController::matches(const Binding& binding,
                             const juce::MidiMessage& message) noexcept
{
    if (binding.channel != message.getChannel())
        return false;
    if (binding.type == MessageType::note)
        return message.isNoteOn(true) && binding.number == message.getNoteNumber();
    if (binding.type == MessageType::controller)
        return message.isController() && binding.number == message.getControllerNumber();
    return message.isPitchWheel();
}

float MidiController::pitchPercentFromMidi(float normalizedValue) noexcept
{
    const auto midiValue = std::clamp(normalizedValue, 0.0f, 1.0f) * 127.0f;
    if (midiValue <= 64.0f)
        return (midiValue / 64.0f - 1.0f) * 16.0f;
    return ((midiValue - 64.0f) / 63.0f) * 16.0f;
}

float MidiController::normalizedPitchValueFromMidi14Bit(int midiValue) noexcept
{
    const auto value = std::clamp(midiValue, 0, 16383);
    if (value <= 8192)
        return (static_cast<float>(value) / 8192.0f * 64.0f) / 127.0f;
    return (64.0f + static_cast<float>(value - 8192) / 8191.0f * 63.0f)
        / 127.0f;
}

float MidiController::relativeJogDeltaFromMidi(int midiValue) noexcept
{
    const auto value = std::clamp(midiValue, 0, 127);
    if (value == 0 || value == 64)
        return 0.0f;
    return static_cast<float>(value < 64 ? value : value - 128);
}

juce::Array<juce::MidiDeviceInfo> MidiController::getAvailableDevices()
{
    return juce::MidiInput::getAvailableDevices();
}

bool MidiController::selectDevice(const juce::String& identifier)
{
    if (midiInput != nullptr)
    {
        midiInput->stop();
        midiInput.reset();
    }
    selectedDeviceIdentifier = identifier;
    preferences->setValue("midi.device", identifier);
    preferences->saveIfNeeded();

    if (identifier.isEmpty())
    {
        setStatus("MIDI input disconnected");
        return true;
    }

    midiInput = juce::MidiInput::openDevice(identifier, this);
    if (midiInput == nullptr)
    {
        setStatus("Could not open the selected MIDI input");
        return false;
    }

    midiInput->start();
    setStatus("Listening for MIDI input");
    return true;
}

juce::String MidiController::getSelectedDeviceIdentifier() const
{
    return selectedDeviceIdentifier;
}

juce::String MidiController::getStatus() const
{
    return status;
}

void MidiController::setActionHandler(ActionHandler handler)
{
    actionHandler = std::move(handler);
}

void MidiController::applyProfile(int profileIndex)
{
    if (profileIndex < 0 || profileIndex >= static_cast<int>(profiles().size()))
    {
        setStatus("Select a valid MIDI profile");
        return;
    }

    bindings.fill(std::nullopt);
    for (const auto& binding : profiles()[static_cast<std::size_t>(profileIndex)].bindings)
        bindings[static_cast<std::size_t>(binding.action)] = binding;

    preferences->setValue("midi.profile", profileIndex);
    savePreferences();
    setStatus(profiles()[static_cast<std::size_t>(profileIndex)].name
              + " profile loaded; verify controls on your device");
}

void MidiController::beginLearn(Action action)
{
    learningAction.store(static_cast<int>(action), std::memory_order_release);
    setStatus("Press or move the control to assign");
}

void MidiController::cancelLearn()
{
    learningAction.store(-1, std::memory_order_release);
    setStatus("MIDI learn cancelled");
}

bool MidiController::isLearning() const noexcept
{
    return learningAction.load(std::memory_order_acquire) >= 0;
}

std::optional<MidiController::Binding> MidiController::getBinding(Action action) const
{
    const auto index = static_cast<std::size_t>(action);
    if (index >= bindings.size())
        return std::nullopt;
    return bindings[index];
}

juce::String MidiController::getBindingDescription(Action action) const
{
    const auto binding = getBinding(action);
    if (!binding.has_value())
        return "Unassigned";

    if (binding->type == MessageType::pitchWheel)
        return "Pitch bend · Ch " + juce::String(binding->channel);
    return (binding->type == MessageType::note ? "Note " : "CC ")
        + juce::String(binding->number) + " · Ch " + juce::String(binding->channel);
}

bool MidiController::exportBindings(const juce::File& file)
{
    juce::String error;
    if (!exportMappingFile(file, bindings, error))
    {
        setStatus(std::move(error));
        return false;
    }
    setStatus("MIDI mapping exported to " + file.getFileName());
    return true;
}

bool MidiController::exportMappingFile(const juce::File& file,
                                       const BindingSet& bindings,
                                       juce::String& error)
{
    if (file == juce::File {} || file.isDirectory())
    {
        error = "Choose a valid MIDI mapping file destination";
        return false;
    }
    auto* root = new juce::DynamicObject();
    root->setProperty("format", "Vyra MIDI Mapping");
    root->setProperty("version", 1);
    juce::Array<juce::var> mappings;
    for (const auto action : getActions())
    {
        const auto& binding = bindings[static_cast<std::size_t>(action)];
        if (!binding.has_value())
            continue;
        auto* mapping = new juce::DynamicObject();
        mapping->setProperty("action", static_cast<int>(action));
        const auto typeName = binding->type == MessageType::note ? "note"
            : binding->type == MessageType::controller ? "cc" : "pitchWheel";
        mapping->setProperty("type", typeName);
        mapping->setProperty("channel", binding->channel);
        mapping->setProperty("number", binding->number);
        mappings.add(juce::var(mapping));
    }
    root->setProperty("mappings", mappings);
    const auto json = juce::JSON::toString(juce::var(root), true);
    if (!file.replaceWithText(json))
    {
        error = "Could not write MIDI mapping file: " + file.getFullPathName();
        return false;
    }
    error.clear();
    return true;
}

bool MidiController::importBindings(const juce::File& file)
{
    juce::String error;
    const auto imported = importMappingFile(file, error);
    if (!imported.has_value())
    {
        setStatus(std::move(error));
        return false;
    }

    bindings = *imported;
    savePreferences();
    setStatus("MIDI mapping imported from " + file.getFileName());
    return true;
}

std::optional<MidiController::BindingSet> MidiController::importMappingFile(
    const juce::File& file, juce::String& error)
{
    if (!file.existsAsFile() || file.getSize() > 1024 * 1024)
    {
        error = "MIDI mapping file is missing or larger than 1 MB";
        return std::nullopt;
    }

    const auto parsed = juce::JSON::parse(file);
    const auto* root = parsed.getDynamicObject();
    if (root == nullptr
        || root->getProperty("format").toString() != "Vyra MIDI Mapping"
        || static_cast<int>(root->getProperty("version")) != 1
        || !root->getProperty("mappings").isArray())
    {
        error = "Invalid MIDI mapping file header or version";
        return std::nullopt;
    }

    const auto* mappings = root->getProperty("mappings").getArray();
    if (mappings == nullptr || mappings->size() > actionCount)
    {
        error = "MIDI mapping contains too many entries";
        return std::nullopt;
    }

    BindingSet imported {};
    for (const auto& item : *mappings)
    {
        const auto* mapping = item.getDynamicObject();
        if (mapping == nullptr)
        {
            error = "MIDI mapping entry must be an object";
            return std::nullopt;
        }

        const auto actionValue = static_cast<int>(mapping->getProperty("action"));
        const auto typeName = mapping->getProperty("type").toString();
        const auto channel = static_cast<int>(mapping->getProperty("channel"));
        const auto number = static_cast<int>(mapping->getProperty("number"));
        if (actionValue < 0 || actionValue >= static_cast<int>(Action::count)
            || (typeName != "note" && typeName != "cc"
                && typeName != "pitchWheel")
            || channel < 1 || channel > 16 || number < 0 || number > 127)
        {
            error = "MIDI mapping contains an out-of-range action or MIDI value";
            return std::nullopt;
        }
        const auto action = static_cast<Action>(actionValue);
        if (typeName == "pitchWheel" && (number != 0 || !isContinuous(action)))
        {
            error = "Pitch bend must be assigned to a continuous action";
            return std::nullopt;
        }
        const auto index = static_cast<std::size_t>(action);
        if (imported[index].has_value())
        {
            error = "MIDI mapping assigns an action more than once";
            return std::nullopt;
        }
        imported[index] = Binding {
            action, typeName == "note" ? MessageType::note
                : typeName == "cc" ? MessageType::controller
                                   : MessageType::pitchWheel,
            channel, number
        };
    }

    for (std::size_t first = 0; first < imported.size(); ++first)
    {
        if (!imported[first].has_value())
            continue;
        for (std::size_t second = first + 1; second < imported.size(); ++second)
        {
            if (imported[second].has_value()
                && imported[first]->type == imported[second]->type
                && imported[first]->channel == imported[second]->channel
                && imported[first]->number == imported[second]->number)
            {
                error = "MIDI mapping assigns one physical control to multiple actions";
                return std::nullopt;
            }
        }
    }

    error.clear();
    return imported;
}

void MidiController::handleIncomingMidiMessage(juce::MidiInput*,
                                                const juce::MidiMessage& message)
{
    const auto event = makeEvent(message);
    if (!event.has_value())
        return;

    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    eventFifo.prepareToWrite(1, start1, size1, start2, size2);
    if (size1 > 0)
        eventQueue[static_cast<std::size_t>(start1)] = *event;
    else if (size2 > 0)
        eventQueue[static_cast<std::size_t>(start2)] = *event;
    else
        droppedEvents.fetch_add(1, std::memory_order_relaxed);
    eventFifo.finishedWrite(size1 + size2);
}

void MidiController::timerCallback()
{
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    eventFifo.prepareToRead(queueCapacity, start1, size1, start2, size2);

    const auto processEvent = [this](const MidiEvent& event)
    {
        const auto target = learningAction.load(std::memory_order_acquire);
        if (target >= 0)
        {
            const auto action = static_cast<Action>(target);
            const auto requiresContinuous = isContinuous(action)
                || action == Action::jogDeck1 || action == Action::jogDeck2;
            const auto isContinuousInput = event.type != MessageType::note;
            const auto isJog = action == Action::jogDeck1 || action == Action::jogDeck2;
            if (requiresContinuous != isContinuousInput
                || (isJog && event.type != MessageType::controller))
            {
                setStatus(requiresContinuous
                    ? isJog ? "Move a MIDI jog wheel or relative encoder"
                            : "Move a MIDI fader, knob, or pitch bend"
                    : "Press a MIDI button for this action");
                return;
            }
            if (event.type == MessageType::note && !event.pressed)
                return;

            const Binding learned { action, event.type, event.channel, event.number };
            for (auto& binding : bindings)
            {
                if (binding.has_value() && binding->type == learned.type
                    && binding->channel == learned.channel
                    && binding->number == learned.number)
                    binding.reset();
            }
            bindings[static_cast<std::size_t>(target)] = learned;
            learningAction.store(-1, std::memory_order_release);
            savePreferences();
            setStatus(getActionName(action) + " assigned to "
                      + getBindingDescription(action));
            return;
        }

        for (const auto action : getActions())
        {
            const auto binding = getBinding(action);
            if (!binding.has_value() || binding->type != event.type
                || binding->channel != event.channel
                || binding->number != event.number)
                continue;
            if (event.type == MessageType::note && !event.pressed)
                continue;

            if (actionHandler != nullptr)
            {
                auto value = 1.0f;
                if (event.type == MessageType::controller)
                    value = static_cast<float>(event.value) / 127.0f;
                else if (event.type == MessageType::pitchWheel)
                    value = normalizedPitchValueFromMidi14Bit(event.value);
                const auto actionValue = action == Action::jogDeck1
                        || action == Action::jogDeck2
                    ? relativeJogDeltaFromMidi(event.value) : value;
                if (actionValue != 0.0f
                    || (action != Action::jogDeck1 && action != Action::jogDeck2))
                    actionHandler(action, actionValue);
            }
        }
    };

    for (int index = 0; index < size1; ++index)
        processEvent(eventQueue[static_cast<std::size_t>(start1 + index)]);
    for (int index = 0; index < size2; ++index)
        processEvent(eventQueue[static_cast<std::size_t>(start2 + index)]);
    eventFifo.finishedRead(size1 + size2);

    const auto dropped = droppedEvents.exchange(0, std::memory_order_relaxed);
    if (dropped > 0)
        setStatus("MIDI event queue full; " + juce::String(dropped) + " event(s) dropped");
}

void MidiController::savePreferences()
{
    for (const auto action : getActions())
    {
        const auto key = "midi.binding." + juce::String(static_cast<int>(action));
        const auto binding = getBinding(action);
        if (!binding.has_value())
        {
            preferences->removeValue(key);
            continue;
        }

        preferences->setValue(key,
            juce::String(static_cast<int>(binding->type)) + ","
            + juce::String(binding->channel) + ","
            + juce::String(binding->number));
    }
    preferences->saveIfNeeded();
}

void MidiController::loadBindings()
{
    for (const auto action : getActions())
    {
        const auto raw = preferences->getValue(
            "midi.binding." + juce::String(static_cast<int>(action)));
        const auto parts = juce::StringArray::fromTokens(raw, ",", {});
        if (parts.size() != 3)
            continue;

        const auto typeValue = parts[0].getIntValue();
        const auto channel = parts[1].getIntValue();
        const auto number = parts[2].getIntValue();
        if ((typeValue != static_cast<int>(MessageType::note)
             && typeValue != static_cast<int>(MessageType::controller)
             && typeValue != static_cast<int>(MessageType::pitchWheel))
            || channel < 1 || channel > 16 || number < 0 || number > 127)
            continue;
        if (typeValue == static_cast<int>(MessageType::pitchWheel)
            && (number != 0 || !isContinuous(action)))
            continue;
        bindings[static_cast<std::size_t>(action)] = Binding {
            action, static_cast<MessageType>(typeValue), channel, number
        };
    }
}

void MidiController::setStatus(juce::String message)
{
    status = std::move(message);
}

bool MidiController::isContinuous(Action action) noexcept
{
    return action == Action::crossfader || action == Action::gainDeck1
        || action == Action::gainDeck2 || action == Action::pitchDeck1
        || action == Action::pitchDeck2 || action == Action::lowEqDeck1
        || action == Action::midEqDeck1 || action == Action::highEqDeck1
        || action == Action::lowEqDeck2 || action == Action::midEqDeck2
        || action == Action::highEqDeck2 || action == Action::filterDeck1
        || action == Action::filterDeck2 || action == Action::echoDeck1
        || action == Action::echoDeck2 || action == Action::masterGain;
}

std::optional<MidiController::MidiEvent> MidiController::makeEvent(
    const juce::MidiMessage& message) noexcept
{
    if (message.isPitchWheel())
    {
        return MidiEvent { MessageType::pitchWheel, message.getChannel(), 0,
                           message.getPitchWheelValue(), true };
    }
    if (message.isController())
    {
        return MidiEvent { MessageType::controller, message.getChannel(),
                           message.getControllerNumber(), message.getControllerValue(),
                           true };
    }
    if (message.isNoteOnOrOff())
    {
        return MidiEvent { MessageType::note, message.getChannel(),
                           message.getNoteNumber(), message.getVelocity(),
                           message.isNoteOn() };
    }
    return std::nullopt;
}
}

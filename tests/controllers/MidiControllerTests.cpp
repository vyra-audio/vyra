#include "vyra/integrations/MidiController.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
}

int main()
{
    using Controller = vyra::integrations::MidiController;
    using Action = Controller::Action;
    using MessageType = Controller::MessageType;

    const auto& profiles = Controller::getProfiles();
    require(profiles.size() >= 6, "common controller profiles are listed");
    require(Controller::getActions().size() == static_cast<std::size_t>(Action::count),
            "each supported action is enumerable");
    for (const auto action : Controller::getActions())
        require(Controller::getActionName(action) != "Unknown action",
                "every enumerable controller action has a display name");

    require(Controller::getActionName(Action::loopDeck1).isNotEmpty()
                && Controller::getActionName(Action::beatJumpBackDeck2).isNotEmpty()
                && Controller::getActionName(Action::syncDeck1).isNotEmpty()
                && Controller::getActionName(Action::headphoneCueDeck2).isNotEmpty()
                && Controller::getActionName(Action::pitchDeck1).isNotEmpty()
                && Controller::getActionName(Action::highEqDeck2).isNotEmpty()
                && Controller::getActionName(Action::filterDeck1).isNotEmpty()
                && Controller::getActionName(Action::echoDeck2).isNotEmpty()
                && Controller::getActionName(Action::masterGain).isNotEmpty()
                && Controller::getActionName(Action::jogDeck1).isNotEmpty(),
            "transport, headphone, pitch, EQ, filter, effect, master, and jog actions are exposed");
    require(std::abs(Controller::pitchPercentFromMidi(0.0f) + 16.0f) < 1.0e-5f
                && std::abs(Controller::pitchPercentFromMidi(64.0f / 127.0f)) < 1.0e-5f
                && std::abs(Controller::pitchPercentFromMidi(1.0f) - 16.0f) < 1.0e-5f,
            "MIDI pitch fader maps its endpoints and center to -16/0/+16 percent");
    require(Controller::pitchPercentFromMidi(-1.0f) == -16.0f
                && Controller::pitchPercentFromMidi(2.0f) == 16.0f,
            "MIDI pitch fader clamps out-of-range values");
    require(std::abs(Controller::pitchPercentFromMidi(
                         Controller::normalizedPitchValueFromMidi14Bit(0)) + 16.0f) < 1.0e-5f
                && std::abs(Controller::pitchPercentFromMidi(
                                Controller::normalizedPitchValueFromMidi14Bit(8192))) < 1.0e-5f
                && std::abs(Controller::pitchPercentFromMidi(
                                Controller::normalizedPitchValueFromMidi14Bit(16383)) - 16.0f) < 1.0e-5f
                && Controller::normalizedPitchValueFromMidi14Bit(-1) == 0.0f
                && Controller::normalizedPitchValueFromMidi14Bit(16384) == 1.0f,
            "14-bit pitch wheel maps exact endpoints and center to pitch fader values");
    require(Controller::relativeJogDeltaFromMidi(1) == 1.0f
                && Controller::relativeJogDeltaFromMidi(127) == -1.0f
                && Controller::relativeJogDeltaFromMidi(0) == 0.0f
                && Controller::relativeJogDeltaFromMidi(64) == 0.0f,
            "relative MIDI jog recognizes forward/reverse ticks and neutral messages");
    const auto mappingFile = juce::File::getSpecialLocation(
        juce::File::tempDirectory).getChildFile("vyra-midi-mapping-test.json");
    mappingFile.deleteFile();
    {
        Controller::BindingSet originalBindings {};
        for (const auto& binding : profiles[3].bindings)
            originalBindings[static_cast<std::size_t>(binding.action)] = binding;
        originalBindings[static_cast<std::size_t>(Action::pitchDeck1)] =
            Controller::Binding { Action::pitchDeck1,
                                  Controller::MessageType::pitchWheel, 1, 0 };
        juce::String mappingError;
        require(Controller::exportMappingFile(mappingFile, originalBindings, mappingError),
                "custom MIDI bindings should export to JSON");
        const auto importedBindings = Controller::importMappingFile(mappingFile, mappingError);
        require(importedBindings.has_value(),
                "valid MIDI mapping JSON should import");
        const auto originalPlayBinding =
            originalBindings[static_cast<std::size_t>(Action::playDeck1)];
        const auto importedPlayBinding =
            (*importedBindings)[static_cast<std::size_t>(Action::playDeck1)];
        require(originalPlayBinding.has_value() && importedPlayBinding.has_value()
                    && originalPlayBinding->type == importedPlayBinding->type
                    && originalPlayBinding->channel == importedPlayBinding->channel
                    && originalPlayBinding->number == importedPlayBinding->number,
                "mapping import should restore assignments");
        const auto importedPitchBinding =
            (*importedBindings)[static_cast<std::size_t>(Action::pitchDeck1)];
        require(importedPitchBinding.has_value()
                    && importedPitchBinding->type == Controller::MessageType::pitchWheel
                    && importedPitchBinding->number == 0,
                "mapping import should preserve 14-bit pitch wheel bindings");
        const auto invalidMapping = mappingFile.getSiblingFile("vyra-midi-invalid-test.json");
        require(invalidMapping.replaceWithText(
                    R"({"format":"Vyra MIDI Mapping","version":1,"mappings":[{"action":999,"type":"note","channel":1,"number":1}]})"),
                "invalid mapping fixture should be written");
        require(!Controller::importMappingFile(invalidMapping, mappingError).has_value(),
                "out-of-range MIDI actions should be rejected");
        require(invalidMapping.replaceWithText(
                    R"({"format":"Vyra MIDI Mapping","version":1,"mappings":[{"action":0,"type":"note","channel":1,"number":7},{"action":1,"type":"note","channel":1,"number":7}]})"),
                "duplicate-control mapping fixture should be written");
        require(!Controller::importMappingFile(invalidMapping, mappingError).has_value(),
                "one MIDI control must not be assigned to multiple actions");
        require(invalidMapping.replaceWithText(
                    R"({"format":"Vyra MIDI Mapping","version":1,"mappings":[{"action":0,"type":"pitchWheel","channel":1,"number":0}]})"),
                "invalid pitch wheel mapping fixture should be written");
        require(!Controller::importMappingFile(invalidMapping, mappingError).has_value(),
                "pitch wheel must only control continuous actions");
        invalidMapping.deleteFile();
        require(!Controller::importMappingFile(
                    mappingFile.getSiblingFile("missing-map.json"), mappingError).has_value(),
                "missing mapping files should fail explicitly");
    }
    mappingFile.deleteFile();

    const auto partyMixII = Controller::getBindingFromProfile(1, Action::playDeck1);
    require(!partyMixII.has_value(),
            "Party Mix II uses MIDI learn rather than an unverified preset");

    const auto ddj400Play = Controller::getBindingFromProfile(3, Action::playDeck1);
    require(ddj400Play.has_value() && ddj400Play->type == MessageType::note
                && ddj400Play->channel == 1 && ddj400Play->number == 0x0b,
            "DDJ-400 deck 1 play mapping matches the published mapping");
    const auto flx4Cue = Controller::getBindingFromProfile(4, Action::cueDeck2);
    require(flx4Cue.has_value() && flx4Cue->channel == 2 && flx4Cue->number == 0x0c,
            "DDJ-FLX4 deck 2 cue mapping is assigned");
    const auto inpulseDeck2 = Controller::getBindingFromProfile(5, Action::playDeck2);
    require(inpulseDeck2.has_value() && inpulseDeck2->channel == 3
                && inpulseDeck2->number == 0x07,
            "Inpulse 500 deck 2 uses its distinct MIDI channel");

    const auto partyMixCrossfader = Controller::getBindingFromProfile(
        2, Action::crossfader);
    require(partyMixCrossfader.has_value()
                && partyMixCrossfader->type == MessageType::controller
                && partyMixCrossfader->channel == 16 && partyMixCrossfader->number == 8,
            "original Party Mix crossfader mapping is assigned");

    const auto playMessage = juce::MidiMessage::noteOn(1, 0x0b, static_cast<juce::uint8>(127));
    require(Controller::matches(*ddj400Play, playMessage),
            "binding recognizes a matching note-on");
    require(!Controller::matches(*ddj400Play,
                                 juce::MidiMessage::noteOn(2, 0x0b,
                                                           static_cast<juce::uint8>(127))),
            "binding rejects a note on another channel");
    require(Controller::matches(*partyMixCrossfader,
                                juce::MidiMessage::controllerEvent(16, 8, 64)),
            "binding recognizes a matching continuous controller");
    const Controller::Binding pitchWheelBinding {
        Action::pitchDeck1, Controller::MessageType::pitchWheel, 1, 0
    };
    require(Controller::matches(
                pitchWheelBinding, juce::MidiMessage::pitchWheel(1, 8192))
                && !Controller::matches(
                    pitchWheelBinding, juce::MidiMessage::pitchWheel(2, 8192)),
            "pitch wheel bindings recognize the correct MIDI channel");

    std::cout << "All MIDI controller tests passed\n";
    return EXIT_SUCCESS;
}

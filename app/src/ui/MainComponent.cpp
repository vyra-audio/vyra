#include "vyra/ui/MainComponent.h"

#include <algorithm>
#include <cmath>

namespace vyra::ui
{
namespace
{
const juce::Colour backgroundColour { 0xff101317 };
const juce::Colour panelColour { 0xff191e24 };
const juce::Colour borderColour { 0xff303840 };
const juce::Colour accentColour { 0xff47d7b0 };
const juce::Colour mutedColour { 0xff8e9aa5 };

struct LibraryColumns
{
    int title = 0;
    int artist = 0;
    int album = 0;
    int bpm = 56;
    int key = 48;
    int length = 66;
};

LibraryColumns getLibraryColumns(int width)
{
    LibraryColumns columns;
    const auto flexibleWidth = std::max(
        0, width - columns.bpm - columns.key - columns.length - 16);
    columns.title = flexibleWidth * 42 / 100;
    columns.artist = flexibleWidth * 30 / 100;
    columns.album = flexibleWidth - columns.title - columns.artist;
    return columns;
}

juce::String formatTime(double seconds)
{
    const auto wholeSeconds = std::max(0, static_cast<int>(seconds));
    return juce::String(wholeSeconds / 60).paddedLeft('0', 2)
        + ":" + juce::String(wholeSeconds % 60).paddedLeft('0', 2);
}

void styleButton(juce::TextButton& button, juce::Colour colour)
{
    button.setColour(juce::TextButton::buttonColourId, colour);
    button.setColour(juce::TextButton::buttonOnColourId, colour.brighter(0.12f));
    button.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffedf2f4));
    button.setColour(juce::TextButton::textColourOnId, juce::Colour(0xffedf2f4));
}

std::array<juce::Rectangle<int>, 2> getOverviewWaveformBounds(
    int width, int height)
{
    auto area = juce::Rectangle<int>(0, 0, width, height).reduced(12);
    area.removeFromTop(34 + 6);
    area.removeFromBottom(52 + 6);
    area.removeFromBottom(std::clamp(
        static_cast<int>(area.getHeight() * 0.34f), 132, 260));
    area.removeFromBottom(6);

    auto lanes = area.removeFromTop(std::clamp(area.getHeight() / 10, 44, 58))
                     .reduced(8, 5);
    constexpr int laneGap = 2;
    constexpr int labelWidth = 42;
    const auto laneHeight = (lanes.getHeight() - laneGap) / 2;
    auto firstLane = lanes.removeFromTop(laneHeight);
    lanes.removeFromTop(laneGap);
    auto secondLane = lanes.removeFromTop(laneHeight);
    firstLane.removeFromLeft(labelWidth);
    secondLane.removeFromLeft(labelWidth);
    return { firstLane, secondLane };
}

}

void VyraLookAndFeel::drawButtonBackground(
    juce::Graphics& graphics, juce::Button& button,
    const juce::Colour& backgroundColour, bool highlighted, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    auto colour = backgroundColour;
    if (!button.isEnabled())
        colour = colour.withAlpha(0.42f);
    else if (down)
        colour = colour.brighter(0.14f);
    else if (highlighted)
        colour = colour.brighter(0.07f);

    graphics.setColour(colour);
    graphics.fillRoundedRectangle(bounds, 3.0f);
    graphics.setColour(button.getToggleState()
                           ? accentColour.withAlpha(0.72f)
                           : borderColour.withAlpha(button.isEnabled() ? 0.72f : 0.35f));
    graphics.drawRoundedRectangle(bounds, 3.0f, 0.75f);
}

void VyraLookAndFeel::drawLinearSlider(
    juce::Graphics& graphics, int x, int y, int width, int height,
    float sliderPosition, float minSliderPosition, float maxSliderPosition,
    juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal
        && style != juce::Slider::LinearVertical)
    {
        juce::LookAndFeel_V4::drawLinearSlider(
            graphics, x, y, width, height, sliderPosition,
            minSliderPosition, maxSliderPosition, style, slider);
        return;
    }

    const auto horizontal = style == juce::Slider::LinearHorizontal;
    const auto start = horizontal ? static_cast<float>(x + 5)
                                  : static_cast<float>(y + 5);
    const auto end = horizontal ? static_cast<float>(x + width - 5)
                                : static_cast<float>(y + height - 5);
    const auto position = std::clamp(sliderPosition, start, end);
    const auto centre = horizontal ? static_cast<float>(y) + height * 0.5f
                                   : static_cast<float>(x) + width * 0.5f;
    graphics.setColour(slider.findColour(juce::Slider::backgroundColourId));
    if (horizontal)
        graphics.fillRoundedRectangle(start, centre - 1.0f, end - start, 2.0f, 1.0f);
    else
        graphics.fillRoundedRectangle(centre - 1.0f, start, 2.0f, end - start, 1.0f);

    graphics.setColour(slider.findColour(juce::Slider::trackColourId));
    if (horizontal)
        graphics.fillRoundedRectangle(start, centre - 1.0f, position - start, 2.0f, 1.0f);
    else
        graphics.fillRoundedRectangle(centre - 1.0f, position, 2.0f, end - position, 1.0f);

    graphics.setColour(slider.findColour(juce::Slider::thumbColourId));
    if (horizontal)
        graphics.fillEllipse(position - 4.0f, centre - 4.0f, 8.0f, 8.0f);
    else
        graphics.fillEllipse(centre - 4.0f, position - 4.0f, 8.0f, 8.0f);
}

void VyraLookAndFeel::drawRotarySlider(
    juce::Graphics& graphics, int x, int y, int width, int height,
    float sliderPosition, float rotaryStartAngle, float rotaryEndAngle,
    juce::Slider& slider)
{
    const auto diameter = static_cast<float>(std::max(
        0, std::min(width, height) - 8));
    const auto bounds = juce::Rectangle<float>(
        static_cast<float>(x) + (static_cast<float>(width) - diameter) * 0.5f,
        static_cast<float>(y) + (static_cast<float>(height) - diameter) * 0.5f,
        diameter, diameter);
    const auto centre = bounds.getCentre();
    const auto radius = diameter * 0.5f;
    const auto angle = rotaryStartAngle
        + sliderPosition * (rotaryEndAngle - rotaryStartAngle);
    juce::Path arc;
    arc.addCentredArc(centre.x, centre.y, radius - 1.0f, radius - 1.0f,
                      0.0f, rotaryStartAngle, rotaryEndAngle, true);
    graphics.setColour(slider.findColour(juce::Slider::rotarySliderOutlineColourId));
    graphics.strokePath(arc, juce::PathStrokeType(2.0f));

    juce::Path valueArc;
    valueArc.addCentredArc(centre.x, centre.y, radius - 1.0f, radius - 1.0f,
                           0.0f, rotaryStartAngle, angle, true);
    graphics.setColour(slider.findColour(juce::Slider::rotarySliderFillColourId));
    graphics.strokePath(valueArc, juce::PathStrokeType(2.0f));

    const auto pointerLength = radius * 0.58f;
    juce::Path pointer;
    pointer.startNewSubPath(centre.x, centre.y);
    pointer.lineTo(centre.x + std::sin(angle) * pointerLength,
                   centre.y - std::cos(angle) * pointerLength);
    graphics.setColour(juce::Colour(0xffe6ecee));
    graphics.strokePath(pointer, juce::PathStrokeType(1.25f));
}

namespace
{

class SettingsComponent final : public juce::Component,
                                private juce::Timer
{
public:
    SettingsComponent(juce::AudioDeviceManager& deviceManager,
                      integrations::DiscordPresence& activity,
                      integrations::MidiController& midi,
                      audio::AudioEngine& audioEngine)
        : manager(deviceManager), discordPresence(activity), midiController(midi),
          engine(audioEngine)
    {
        driverLabel.setText("AUDIO BACKEND", juce::dontSendNotification);
        outputLabel.setText("OUTPUT DEVICE", juce::dontSendNotification);
        headphoneLabel.setText("HEADPHONE MONITORING", juce::dontSendNotification);
        headphoneLabel.setColour(juce::Label::textColourId, accentColour);
        headphoneLabel.setFont(juce::Font(12.0f, juce::Font::bold));
        headphoneToggle.setButtonText(
            "Enable separate headphone cue output (requires 4+ device outputs)");
        headphoneToggle.setToggleState(engine.isHeadphoneMonitoringEnabled(),
                                       juce::dontSendNotification);
        headphoneToggle.onClick = [this]
        {
            configureHeadphoneMonitoring(headphoneToggle.getToggleState());
        };
        headphonePairLabel.setText("HEADPHONE OUTPUT PAIR", juce::dontSendNotification);
        headphonePairLabel.setColour(juce::Label::textColourId, mutedColour);
        headphonePairLabel.setFont(juce::Font(10.0f, juce::Font::bold));
        headphoneMixLabel.setText("CUE / MASTER MIX", juce::dontSendNotification);
        headphoneMixLabel.setColour(juce::Label::textColourId, mutedColour);
        headphoneMixLabel.setFont(juce::Font(10.0f, juce::Font::bold));
        headphoneMixSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        headphoneMixSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        headphoneMixSlider.setRange(0.0, 1.0, 0.01);
        headphoneMixSlider.setValue(0.5, juce::dontSendNotification);
        headphoneMixSlider.setColour(juce::Slider::trackColourId, accentColour);
        headphoneMixSlider.setColour(juce::Slider::backgroundColourId,
                                     juce::Colour(0xff303840));
        headphoneMixSlider.setTooltip("0% = cue only, 100% = master only.");
        headphoneMixSlider.onValueChange = [this]
        {
            engine.setHeadphoneMix(static_cast<float>(headphoneMixSlider.getValue()));
        };
        statusLabel.setColour(juce::Label::textColourId, mutedColour);
        statusLabel.setMinimumHorizontalScale(0.75f);
        supportTitle.setText("SUPPORT VYRA", juce::dontSendNotification);
        supportTitle.setColour(juce::Label::textColourId, accentColour);
        supportTitle.setFont(juce::Font(12.0f, juce::Font::bold));
        supportDescription.setText(
            "Fund development or contribute code, documentation, tests, and translations.",
            juce::dontSendNotification);
        supportDescription.setColour(juce::Label::textColourId, mutedColour);
        supportDescription.setFont(juce::Font(12.0f));
        supportDescription.setMinimumHorizontalScale(0.8f);
        presenceToggle.setButtonText("Share track title and progress on Discord");
        presenceToggle.setToggleState(discordPresence.isEnabled(), juce::dontSendNotification);
        presenceToggle.onClick = [this]
        {
            discordPresence.setEnabled(presenceToggle.getToggleState());
            updatePresenceStatus();
        };
        presenceStatus.setColour(juce::Label::textColourId, mutedColour);
        presenceStatus.setFont(juce::Font(11.0f));
        presenceStatus.setMinimumHorizontalScale(0.75f);
        midiTitle.setText("MIDI CONTROLLER", juce::dontSendNotification);
        midiTitle.setColour(juce::Label::textColourId, accentColour);
        midiTitle.setFont(juce::Font(12.0f, juce::Font::bold));
        midiDeviceLabel.setText("INPUT DEVICE", juce::dontSendNotification);
        midiProfileLabel.setText("CONTROLLER PROFILE", juce::dontSendNotification);
        midiActionLabel.setText("ASSIGN CONTROL", juce::dontSendNotification);
        for (auto* label : { &midiDeviceLabel, &midiProfileLabel, &midiActionLabel })
        {
            label->setColour(juce::Label::textColourId, mutedColour);
            label->setFont(juce::Font(10.0f, juce::Font::bold));
        }
        midiStatus.setColour(juce::Label::textColourId, mutedColour);
        midiStatus.setFont(juce::Font(11.0f));
        midiBinding.setColour(juce::Label::textColourId, mutedColour);
        midiBinding.setFont(juce::Font(11.0f));
        midiBinding.setMinimumHorizontalScale(0.75f);
        refreshMidiButton.setButtonText("REFRESH");
        learnButton.setButtonText("LEARN");
        importMidiButton.setButtonText("IMPORT MAP");
        exportMidiButton.setButtonText("EXPORT MAP");
        for (auto* button : { &refreshMidiButton, &learnButton,
                              &importMidiButton, &exportMidiButton })
            styleButton(*button, juce::Colour(0xff29323a));
        for (const auto& profile : integrations::MidiController::getProfiles())
            midiProfileBox.addItem(profile.name, midiProfileBox.getNumItems() + 1);
        int actionItem = 1;
        for (const auto action : integrations::MidiController::getActions())
            midiActionBox.addItem(integrations::MidiController::getActionName(action),
                                  actionItem++);
        midiDeviceBox.setSelectedId(1, juce::dontSendNotification);
        midiProfileBox.setSelectedId(1, juce::dontSendNotification);
        midiActionBox.setSelectedId(1, juce::dontSendNotification);

        donationButton.setButtonText("FINANCIAL SUPPORT · LINK PENDING");
        contributionButton.setButtonText("CONTRIBUTE · LINK PENDING");
        styleButton(donationButton, panelColour);
        styleButton(contributionButton, panelColour);
        donationButton.setEnabled(false);
        contributionButton.setEnabled(false);

        addAndMakeVisible(driverLabel);
        addAndMakeVisible(outputLabel);
        addAndMakeVisible(statusLabel);
        addAndMakeVisible(headphoneLabel);
        addAndMakeVisible(headphoneToggle);
        addAndMakeVisible(headphonePairLabel);
        addAndMakeVisible(headphonePairBox);
        addAndMakeVisible(headphoneMixLabel);
        addAndMakeVisible(headphoneMixSlider);
        addAndMakeVisible(supportTitle);
        addAndMakeVisible(supportDescription);
        addAndMakeVisible(donationButton);
        addAndMakeVisible(contributionButton);
        addAndMakeVisible(presenceToggle);
        addAndMakeVisible(presenceStatus);
        addAndMakeVisible(midiTitle);
        addAndMakeVisible(midiDeviceLabel);
        addAndMakeVisible(midiProfileLabel);
        addAndMakeVisible(midiActionLabel);
        addAndMakeVisible(midiDeviceBox);
        addAndMakeVisible(midiProfileBox);
        addAndMakeVisible(midiActionBox);
        addAndMakeVisible(refreshMidiButton);
        addAndMakeVisible(learnButton);
        addAndMakeVisible(importMidiButton);
        addAndMakeVisible(exportMidiButton);
        addAndMakeVisible(midiBinding);
        addAndMakeVisible(midiStatus);
        addAndMakeVisible(driverBox);
        addAndMakeVisible(outputBox);

        driverBox.onChange = [this] { changeBackend(); };
        outputBox.onChange = [this] { changeOutput(); };
        headphonePairBox.onChange = [this]
        {
            if (headphoneToggle.getToggleState())
                configureHeadphoneMonitoring(true);
        };
        midiDeviceBox.onChange = [this] { changeMidiDevice(); };
        midiProfileBox.onChange = [this]
        {
            const auto selectedId = midiProfileBox.getSelectedId();
            if (selectedId > 0)
                midiController.applyProfile(selectedId - 1);
            updateMidiStatus();
        };
        midiActionBox.onChange = [this] { updateMidiStatus(); };
        refreshMidiButton.onClick = [this] { refreshMidiDevices(); };
        importMidiButton.onClick = [this] { chooseMidiMappingFile(false); };
        exportMidiButton.onClick = [this] { chooseMidiMappingFile(true); };
        learnButton.onClick = [this]
        {
            if (midiController.isLearning())
            {
                midiController.cancelLearn();
            }
            else
            {
                const auto selectedId = midiActionBox.getSelectedId();
                if (selectedId > 0)
                {
                    const auto action = integrations::MidiController::getActions()[
                        static_cast<std::size_t>(selectedId - 1)];
                    midiController.beginLearn(action);
                }
            }
            updateMidiStatus();
        };

        for (auto* box : { &driverBox, &outputBox, &midiDeviceBox,
                           &midiProfileBox, &midiActionBox, &headphonePairBox })
        {
            box->setColour(juce::ComboBox::backgroundColourId, panelColour);
            box->setColour(juce::ComboBox::textColourId, juce::Colour(0xffedf2f4));
            box->setColour(juce::ComboBox::outlineColourId, borderColour);
        }

        refreshBackends();
        refreshHeadphonePairs();
        refreshMidiDevices();
        updatePresenceStatus();
        updateMidiStatus();
        startTimerHz(2);
    }

    ~SettingsComponent() override
    {
        stopTimer();
    }

    void paint(juce::Graphics& graphics) override
    {
        graphics.fillAll(backgroundColour);
        graphics.setColour(panelColour);
        graphics.fillRoundedRectangle(supportBounds.toFloat(), 6.0f);
        graphics.setColour(borderColour);
        graphics.drawRoundedRectangle(supportBounds.toFloat().reduced(0.5f), 6.0f, 1.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(22);
        driverLabel.setBounds(area.removeFromTop(18));
        driverBox.setBounds(area.removeFromTop(34));
        area.removeFromTop(12);
        outputLabel.setBounds(area.removeFromTop(18));
        outputBox.setBounds(area.removeFromTop(34));
        area.removeFromTop(12);
        statusLabel.setBounds(area.removeFromTop(32));
        area.removeFromTop(6);
        headphoneLabel.setBounds(area.removeFromTop(18));
        headphoneToggle.setBounds(area.removeFromTop(24));
        auto headphoneRow = area.removeFromTop(28);
        headphonePairLabel.setBounds(headphoneRow.removeFromLeft(170));
        headphonePairBox.setBounds(headphoneRow);
        auto mixRow = area.removeFromTop(28);
        headphoneMixLabel.setBounds(mixRow.removeFromLeft(170));
        headphoneMixSlider.setBounds(mixRow);
        area.removeFromTop(6);
        midiTitle.setBounds(area.removeFromTop(20));
        midiDeviceLabel.setBounds(area.removeFromTop(16));
        auto midiDeviceRow = area.removeFromTop(32);
        refreshMidiButton.setBounds(midiDeviceRow.removeFromRight(76));
        midiDeviceRow.removeFromRight(6);
        midiDeviceBox.setBounds(midiDeviceRow);
        area.removeFromTop(6);
        midiProfileLabel.setBounds(area.removeFromTop(16));
        midiProfileBox.setBounds(area.removeFromTop(32));
        area.removeFromTop(6);
        midiActionLabel.setBounds(area.removeFromTop(16));
        auto midiActionRow = area.removeFromTop(32);
        learnButton.setBounds(midiActionRow.removeFromRight(76));
        midiActionRow.removeFromRight(6);
        midiActionBox.setBounds(midiActionRow);
        midiBinding.setBounds(area.removeFromTop(20));
        midiStatus.setBounds(area.removeFromTop(20));
        auto mappingRow = area.removeFromTop(30);
        importMidiButton.setBounds(mappingRow.removeFromLeft(
            std::min(120, mappingRow.getWidth() / 2)));
        mappingRow.removeFromLeft(6);
        exportMidiButton.setBounds(mappingRow);
        area.removeFromTop(8);
        supportBounds = area.removeFromTop(132);
        auto supportArea = supportBounds.reduced(14, 8);
        supportTitle.setBounds(supportArea.removeFromTop(18));
        supportArea.removeFromTop(2);
        supportDescription.setBounds(supportArea.removeFromTop(32));
        supportArea.removeFromTop(4);
        auto buttons = supportArea.removeFromTop(30);
        const auto buttonWidth = (buttons.getWidth() - 8) / 2;
        donationButton.setBounds(buttons.removeFromLeft(buttonWidth));
        buttons.removeFromLeft(8);
        contributionButton.setBounds(buttons);
        area.removeFromTop(10);
        presenceToggle.setBounds(area.removeFromTop(24));
        presenceStatus.setBounds(area.removeFromTop(24));
    }

private:
    void timerCallback() override
    {
        updatePresenceStatus();
        updateMidiStatus();
    }

    void updatePresenceStatus()
    {
        presenceStatus.setText(discordPresence.getStatus(), juce::dontSendNotification);
    }

    void refreshMidiDevices()
    {
        const auto selectedIdentifier = midiController.getSelectedDeviceIdentifier();
        midiDeviceBox.clear(juce::dontSendNotification);
        midiDeviceBox.addItem("No MIDI input", 1);

        auto selectedId = 1;
        const auto devices = integrations::MidiController::getAvailableDevices();
        for (int index = 0; index < devices.size(); ++index)
        {
            const auto itemId = index + 2;
            midiDeviceBox.addItem(devices[index].name, itemId);
            if (devices[index].identifier == selectedIdentifier)
                selectedId = itemId;
        }
        midiDeviceBox.setSelectedId(selectedId, juce::dontSendNotification);
        if (selectedIdentifier.isNotEmpty() && selectedId == 1)
        {
            const auto unavailableId = devices.size() + 2;
            midiDeviceBox.addItem("Saved MIDI device unavailable", unavailableId);
            midiDeviceBox.setSelectedId(unavailableId, juce::dontSendNotification);
        }
        updateMidiStatus();
    }

    void changeMidiDevice()
    {
        const auto selectedId = midiDeviceBox.getSelectedId();
        if (selectedId <= 0)
            return;
        if (selectedId == 1)
        {
            const auto disconnected = midiController.selectDevice({});
            juce::ignoreUnused(disconnected);
            updateMidiStatus();
            return;
        }
        const auto devices = integrations::MidiController::getAvailableDevices();
        const auto index = selectedId - 2;
        if (index >= 0 && index < devices.size())
        {
            const auto opened = midiController.selectDevice(devices[index].identifier);
            if (!opened)
                midiStatus.setText(midiController.getStatus(), juce::dontSendNotification);
            updateMidiStatus();
        }
    }

    void updateMidiStatus()
    {
        const auto selectedId = midiActionBox.getSelectedId();
        if (selectedId > 0)
        {
            const auto action = integrations::MidiController::getActions()[
                static_cast<std::size_t>(selectedId - 1)];
            midiBinding.setText("Current mapping: "
                                + midiController.getBindingDescription(action),
                                juce::dontSendNotification);
        }
        midiStatus.setText(midiController.getStatus(), juce::dontSendNotification);
        learnButton.setButtonText(midiController.isLearning() ? "CANCEL" : "LEARN");
    }

    void chooseMidiMappingFile(bool exportMapping)
    {
        midiMappingChooser = std::make_unique<juce::FileChooser>(
            exportMapping ? "Export Vyra MIDI mapping" : "Import Vyra MIDI mapping",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                .getChildFile("Vyra MIDI Mapping.json"),
            "*.json");
        juce::Component::SafePointer<SettingsComponent> safeThis(this);
        const auto flags = exportMapping
            ? juce::FileBrowserComponent::saveMode
                | juce::FileBrowserComponent::canSelectFiles
                | juce::FileBrowserComponent::warnAboutOverwriting
            : juce::FileBrowserComponent::openMode
                | juce::FileBrowserComponent::canSelectFiles;
        midiMappingChooser->launchAsync(flags,
            [safeThis, exportMapping](const juce::FileChooser& chooser)
            {
                if (safeThis == nullptr)
                    return;
                const auto file = chooser.getResult();
                safeThis->midiMappingChooser.reset();
                if (file == juce::File {})
                    return;
                const auto succeeded = exportMapping
                    ? safeThis->midiController.exportBindings(file)
                    : safeThis->midiController.importBindings(file);
                if (succeeded && !exportMapping)
                    safeThis->midiProfileBox.setSelectedId(1, juce::dontSendNotification);
                safeThis->updateMidiStatus();
            });
    }

    void refreshBackends()
    {
        const auto& types = manager.getAvailableDeviceTypes();
        const auto currentTypeName = manager.getCurrentAudioDeviceType();
        driverBox.clear(juce::dontSendNotification);

        int selectedId = 0;
        for (int index = 0; index < types.size(); ++index)
        {
            const auto* type = types[index];
            if (type == nullptr)
                continue;

            const auto itemId = index + 1;
            driverBox.addItem(type->getTypeName(), itemId);
            if (type->getTypeName() == currentTypeName)
                selectedId = itemId;
        }

        if (selectedId == 0 && driverBox.getNumItems() > 0)
            selectedId = 1;
        driverBox.setSelectedId(selectedId, juce::dontSendNotification);
        refreshOutputs();
    }

    void changeBackend()
    {
        const auto selectedType = driverBox.getText();
        if (selectedType.isEmpty())
            return;

        if (engine.isRecording())
            engine.stopRecording();
        engine.setHeadphoneMonitoringEnabled(false);
        headphoneToggle.setToggleState(false, juce::dontSendNotification);
        manager.setCurrentAudioDeviceType(selectedType, true);
        refreshOutputs();
    }

    void refreshOutputs()
    {
        auto* type = manager.getCurrentDeviceTypeObject();
        outputBox.clear(juce::dontSendNotification);
        if (type == nullptr)
        {
            outputBox.setEnabled(false);
            updateStatus();
            return;
        }

        type->scanForDevices();
        const auto names = type->getDeviceNames(false);
        for (int index = 0; index < names.size(); ++index)
            outputBox.addItem(names[index], index + 1);

        const auto setup = manager.getAudioDeviceSetup();
        auto selectedIndex = names.indexOf(setup.outputDeviceName);
        if (selectedIndex < 0)
            selectedIndex = type->getDefaultDeviceIndex(false);

        outputBox.setEnabled(!names.isEmpty());
        outputBox.setSelectedId(selectedIndex >= 0 ? selectedIndex + 1 : 0,
                                juce::dontSendNotification);
        refreshHeadphonePairs();
        updateStatus();
    }

    void refreshHeadphonePairs()
    {
        headphonePairBox.clear(juce::dontSendNotification);
        auto* device = manager.getCurrentAudioDevice();
        if (device == nullptr)
        {
            headphonePairBox.setEnabled(false);
            headphoneToggle.setEnabled(false);
            headphoneToggle.setToggleState(false, juce::dontSendNotification);
            engine.setHeadphoneMonitoringEnabled(false);
            return;
        }

        const auto channelNames = device->getOutputChannelNames();
        int selectedId = 0;
        for (int firstChannel = 2; firstChannel + 1 < channelNames.size(); firstChannel += 2)
        {
            const auto itemId = firstChannel + 1;
            headphonePairBox.addItem(
                "Output " + juce::String(firstChannel + 1) + " / "
                    + juce::String(firstChannel + 2),
                itemId);
            if (firstChannel == engine.getHeadphoneOutputStartChannel())
                selectedId = itemId;
        }
        headphonePairBox.setEnabled(headphonePairBox.getNumItems() > 0);
        headphoneToggle.setEnabled(channelNames.size() >= 4);
        if (selectedId == 0 && headphonePairBox.getNumItems() > 0)
            selectedId = headphonePairBox.getItemId(0);
        headphonePairBox.setSelectedId(selectedId, juce::dontSendNotification);
        if (channelNames.size() < 4)
        {
            headphoneToggle.setToggleState(false, juce::dontSendNotification);
            engine.setHeadphoneMonitoringEnabled(false);
        }
    }

    void configureHeadphoneMonitoring(bool enabled)
    {
        auto* device = manager.getCurrentAudioDevice();
        if (!enabled)
        {
            const auto wasEnabled = engine.isHeadphoneMonitoringEnabled();
            engine.setHeadphoneMonitoringEnabled(false);
            if (wasEnabled)
            {
                if (engine.isRecording())
                    engine.stopRecording();
                auto setup = manager.getAudioDeviceSetup();
                setup.useDefaultOutputChannels = true;
                const auto error = manager.setAudioDeviceSetup(setup, true);
                updateStatus(error);
            }
            return;
        }

        if (device == nullptr)
        {
            headphoneToggle.setToggleState(false, juce::dontSendNotification);
            updateStatus("Headphone cue requires an active audio output device.");
            return;
        }

        const auto startChannel = headphonePairBox.getSelectedId() - 1;
        if (startChannel < 2 || startChannel + 1 >= device->getOutputChannelNames().size())
        {
            headphoneToggle.setToggleState(false, juce::dontSendNotification);
            updateStatus("Select an available stereo output pair for headphones.");
            return;
        }

        auto setup = manager.getAudioDeviceSetup();
        setup.useDefaultOutputChannels = false;
        setup.outputChannels.clear();
        setup.outputChannels.setBit(0, true);
        setup.outputChannels.setBit(1, true);
        setup.outputChannels.setBit(startChannel, true);
        setup.outputChannels.setBit(startChannel + 1, true);
        if (engine.isRecording())
            engine.stopRecording();
        const auto error = manager.setAudioDeviceSetup(setup, true);
        if (error.isNotEmpty())
        {
            headphoneToggle.setToggleState(false, juce::dontSendNotification);
            engine.setHeadphoneMonitoringEnabled(false);
            updateStatus("Could not activate 4-channel output: " + error);
            return;
        }

        engine.setHeadphoneOutputStartChannel(startChannel);
        engine.setHeadphoneMonitoringEnabled(true);
        statusLabel.setText("Master: outputs 1/2 · Headphones: outputs "
                                + juce::String(startChannel + 1) + "/"
                                + juce::String(startChannel + 2),
                            juce::dontSendNotification);
    }

    void changeOutput()
    {
        const auto selectedId = outputBox.getSelectedId();
        if (selectedId <= 0)
            return;

        if (engine.isRecording())
            engine.stopRecording();
        auto setup = manager.getAudioDeviceSetup();
        setup.outputDeviceName = outputBox.getText();
        setup.useDefaultOutputChannels = true;
        engine.setHeadphoneMonitoringEnabled(false);
        headphoneToggle.setToggleState(false, juce::dontSendNotification);
        const auto error = manager.setAudioDeviceSetup(setup, true);
        refreshHeadphonePairs();
        updateStatus(error);
    }

    void updateStatus(const juce::String& error = {})
    {
        if (error.isNotEmpty())
        {
            statusLabel.setText("Device error: " + error, juce::dontSendNotification);
            return;
        }

        if (auto* device = manager.getCurrentAudioDevice())
        {
            statusLabel.setText(device->getName() + "  |  "
                                    + juce::String(device->getCurrentSampleRate() / 1000.0, 1)
                                    + " kHz / " + juce::String(device->getCurrentBufferSizeSamples())
                                    + " samples",
                                juce::dontSendNotification);
        }
        else
        {
            statusLabel.setText("No active output device", juce::dontSendNotification);
        }
    }

    juce::AudioDeviceManager& manager;
    integrations::DiscordPresence& discordPresence;
    integrations::MidiController& midiController;
    audio::AudioEngine& engine;
    juce::Label headphoneLabel;
    juce::ToggleButton headphoneToggle;
    juce::Label headphonePairLabel;
    juce::ComboBox headphonePairBox;
    juce::Label headphoneMixLabel;
    juce::Slider headphoneMixSlider;
    juce::Label driverLabel { {}, "AUDIO BACKEND" };
    juce::Label outputLabel { {}, "OUTPUT DEVICE" };
    juce::Label statusLabel;
    juce::Label midiTitle;
    juce::Label midiDeviceLabel;
    juce::Label midiProfileLabel;
    juce::Label midiActionLabel;
    juce::Label midiBinding;
    juce::Label midiStatus;
    juce::ComboBox midiDeviceBox;
    juce::ComboBox midiProfileBox;
    juce::ComboBox midiActionBox;
    juce::TextButton refreshMidiButton;
    juce::TextButton learnButton;
    juce::TextButton importMidiButton;
    juce::TextButton exportMidiButton;
    std::unique_ptr<juce::FileChooser> midiMappingChooser;
    juce::Label supportTitle;
    juce::Label supportDescription;
    juce::ComboBox driverBox;
    juce::ComboBox outputBox;
    juce::TextButton donationButton;
    juce::TextButton contributionButton;
    juce::ToggleButton presenceToggle;
    juce::Label presenceStatus;
    juce::Rectangle<int> supportBounds;
};

class CollectionSidebar final : public juce::Component,
                               private juce::ListBoxModel
{
public:
    explicit CollectionSidebar(std::function<void(int)> selectionChanged)
        : onSelectionChanged(std::move(selectionChanged))
    {
        list.setModel(this);
        list.setRowHeight(25);
        list.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff111416));
        addAndMakeVisible(list);
    }

    ~CollectionSidebar() override
    {
        list.setModel(nullptr);
    }

    void setItems(std::vector<juce::String> newItems, int selectedIndex)
    {
        updating = true;
        items = std::move(newItems);
        list.updateContent();
        selected = std::clamp(selectedIndex, 0,
                              std::max(0, static_cast<int>(items.size()) - 1));
        list.selectRow(selected, juce::dontSendNotification);
        updating = false;
        repaint();
    }

    void resized() override
    {
        list.setBounds(getLocalBounds().withTrimmedTop(24));
    }

    void paint(juce::Graphics& graphics) override
    {
        graphics.fillAll(juce::Colour(0xff111416));
        graphics.setColour(juce::Colour(0xff8e989c));
        graphics.setFont(juce::Font(10.0f, juce::Font::bold));
        graphics.drawText("COLLECTIONS", 10, 4, getWidth() - 20, 16,
                          juce::Justification::centredLeft, false);
        graphics.setColour(juce::Colour(0xff2e3437));
        graphics.drawVerticalLine(getWidth() - 1, 0.0f,
                                  static_cast<float>(getHeight()));
    }

private:
    int getNumRows() override
    {
        return static_cast<int>(items.size());
    }

    void paintListBoxItem(int row, juce::Graphics& graphics, int width, int height,
                          bool rowIsSelected) override
    {
        if (row < 0 || static_cast<std::size_t>(row) >= items.size())
            return;
        if (rowIsSelected)
        {
            graphics.fillAll(juce::Colour(0xff1a2829));
            graphics.setColour(accentColour);
            graphics.fillRect(0, 4, 2, height - 8);
        }
        graphics.setColour(rowIsSelected ? juce::Colours::white
                                         : juce::Colour(0xffbcc2c4));
        graphics.setFont(juce::Font(12.0f, rowIsSelected
                                                ? juce::Font::bold
                                                : juce::Font::plain));
        graphics.drawText(items[static_cast<std::size_t>(row)], 11, 0,
                          width - 18, height, juce::Justification::centredLeft, true);
    }

    void selectedRowsChanged(int row) override
    {
        if (updating || row < 0 || row == selected
            || static_cast<std::size_t>(row) >= items.size())
            return;
        selected = row;
        if (onSelectionChanged)
            onSelectionChanged(row);
    }

    juce::ListBox list;
    std::vector<juce::String> items;
    std::function<void(int)> onSelectionChanged;
    int selected = 0;
    bool updating = false;
};

class TrackTableHeader final : public juce::Component
{
public:
    void setRecentPlays(bool showingRecent)
    {
        recentPlays = showingRecent;
        repaint();
    }

    void paint(juce::Graphics& graphics) override
    {
        graphics.fillAll(juce::Colour(0xff1a1e20));
        graphics.setColour(juce::Colour(0xff889397));
        graphics.setFont(juce::Font(10.0f, juce::Font::bold));
        const auto columns = getLibraryColumns(getWidth());
        graphics.drawText("TRACK", 8, 0, columns.title - 10, getHeight(),
                          juce::Justification::centredLeft, false);
        graphics.drawText("ARTIST", 8 + columns.title, 0,
                          columns.artist - 8, getHeight(),
                          juce::Justification::centredLeft, false);
        graphics.drawText("ALBUM", 8 + columns.title + columns.artist, 0,
                          columns.album - 8, getHeight(),
                          juce::Justification::centredLeft, false);
        auto right = getWidth() - 8;
        graphics.drawText(recentPlays ? "PLAYED" : "LENGTH",
                          right - columns.length, 0, columns.length, getHeight(),
                          juce::Justification::centred, false);
        right -= columns.length;
        graphics.drawText("KEY", right - columns.key, 0, columns.key, getHeight(),
                          juce::Justification::centred, false);
        right -= columns.key;
        graphics.drawText("BPM", right - columns.bpm, 0, columns.bpm, getHeight(),
                          juce::Justification::centred, false);
        graphics.setColour(juce::Colour(0x48303840));
        graphics.drawHorizontalLine(getHeight() - 1, 0.0f,
                                    static_cast<float>(getWidth()));
    }

private:
    bool recentPlays = false;
};

class LibraryPanel final : public juce::Component,
                           private juce::ListBoxModel
{
public:
    LibraryPanel(library::TrackLibrary& trackLibrary,
                 std::function<void(std::size_t, library::Track)> loadTrack)
        : library(trackLibrary), loadTrackForDeck(std::move(loadTrack))
    {
        searchBox.setTextToShowWhenEmpty("Search title, artist, album, genre, or path",
                                         mutedColour);
        searchBox.setColour(juce::TextEditor::backgroundColourId, panelColour);
        searchBox.setColour(juce::TextEditor::outlineColourId, borderColour);
        searchBox.setColour(juce::TextEditor::focusedOutlineColourId, accentColour);
        searchBox.onTextChange = [this] { refreshTracks(); };
        scanButton.onClick = [this]
        {
            if (isRecentPlaysSelected())
                refreshTracks();
            else
                chooseFolder();
        };
        collections = std::make_unique<CollectionSidebar>(
            [this](int index)
            {
                selectedCollection = index;
                refreshTracks();
            });
        playlistNameBox.setTextToShowWhenEmpty("New playlist name", mutedColour);
        playlistNameBox.setColour(juce::TextEditor::backgroundColourId, panelColour);
        playlistNameBox.setColour(juce::TextEditor::outlineColourId, borderColour);
        playlistNameBox.setColour(juce::TextEditor::focusedOutlineColourId, accentColour);
        createPlaylistButton.onClick = [this] { createPlaylist(); };
        addTrackButton.onClick = [this] { updateSelectedTrackInPlaylist(true); };
        removeTrackButton.onClick = [this] { updateSelectedTrackInPlaylist(false); };
        deckBox.addItem("Load to Deck 1", 1);
        deckBox.addItem("Load to Deck 2", 2);
        deckBox.setSelectedId(1, juce::dontSendNotification);
        refreshPlaylists({});
        status.setColour(juce::Label::textColourId, mutedColour);
        libraryTitle.setText("MUSIC LIBRARY", juce::dontSendNotification);
        libraryTitle.setColour(juce::Label::textColourId, juce::Colour(0xffd8e0e2));
        libraryTitle.setFont(juce::Font(10.0f, juce::Font::bold));
        list.setModel(this);
        list.setRowHeight(34);
        list.setColour(juce::ListBox::backgroundColourId, backgroundColour);

        addAndMakeVisible(searchBox);
        addAndMakeVisible(libraryTitle);
        addAndMakeVisible(scanButton);
        addAndMakeVisible(deckBox);
        addAndMakeVisible(collections.get());
        addAndMakeVisible(tableHeader);
        addAndMakeVisible(playlistNameBox);
        addAndMakeVisible(createPlaylistButton);
        addAndMakeVisible(addTrackButton);
        addAndMakeVisible(removeTrackButton);
        addAndMakeVisible(status);
        addAndMakeVisible(list);
        refreshTracks();
    }

    ~LibraryPanel() override
    {
        list.setModel(nullptr);
        removeChildComponent(collections.get());
        collections.reset();
    }

    void paint(juce::Graphics& graphics) override
    {
        graphics.fillAll(backgroundColour);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);
        auto toolbar = area.removeFromTop(28);
        libraryTitle.setBounds(toolbar.removeFromLeft(112));
        toolbar.removeFromLeft(8);
        scanButton.setBounds(toolbar.removeFromRight(std::min(100, toolbar.getWidth() / 4)));
        toolbar.removeFromRight(6);
        searchBox.setBounds(toolbar);
        area.removeFromTop(6);
        auto sidebar = area.removeFromLeft(std::min(152, area.getWidth() / 4));
        collections->setBounds(sidebar);
        area.removeFromLeft(8);
        auto content = area;
        auto playlistRow = content.removeFromTop(26);
        deckBox.setBounds(playlistRow.removeFromRight(
            std::min(130, playlistRow.getWidth() / 5)));
        playlistRow.removeFromRight(4);
        removeTrackButton.setBounds(playlistRow.removeFromRight(
            std::min(76, playlistRow.getWidth() / 6)));
        playlistRow.removeFromRight(4);
        addTrackButton.setBounds(playlistRow.removeFromRight(
            std::min(92, playlistRow.getWidth() / 5)));
        playlistRow.removeFromRight(4);
        createPlaylistButton.setBounds(playlistRow.removeFromRight(
            std::min(68, playlistRow.getWidth() / 5)));
        playlistRow.removeFromRight(4);
        playlistNameBox.setBounds(playlistRow);
        content.removeFromTop(3);
        tableHeader.setBounds(content.removeFromTop(20));
        content.removeFromTop(3);
        status.setBounds(content.removeFromBottom(18));
        content.removeFromBottom(3);
        list.setBounds(content);
    }

private:
    int getNumRows() override
    {
        return static_cast<int>(tracks.size());
    }

    void paintListBoxItem(int row, juce::Graphics& graphics, int width, int height,
                          bool rowIsSelected) override
    {
        if (row < 0 || static_cast<std::size_t>(row) >= tracks.size())
            return;

        if (rowIsSelected)
            graphics.fillAll(juce::Colour(0xff1a2829));
        else if ((row & 1) != 0)
            graphics.fillAll(juce::Colour(0xff15191b));

        const auto& track = tracks[static_cast<std::size_t>(row)];
        const auto missing = !juce::File(track.path).existsAsFile();
        const auto showingRecentPlay = track.lastPlayedAt.isNotEmpty();
        const auto columns = getLibraryColumns(width);
        const auto textColour = missing ? juce::Colour(0xffff6b63)
            : rowIsSelected ? juce::Colour(0xfff0f4f4) : juce::Colour(0xffd5d9da);
        graphics.setColour(textColour);
        graphics.setFont(juce::Font(12.0f, juce::Font::plain));
        graphics.drawText((missing ? "[MISSING] " : "") + track.title,
                          8, 0, columns.title - 10, height,
                          juce::Justification::centredLeft, true);
        graphics.setColour(rowIsSelected ? juce::Colour(0xffd4e9f1)
                                         : mutedColour);
        graphics.setFont(juce::Font(12.0f));
        graphics.drawText(showingRecentPlay
                              ? track.artist + " · Deck "
                                    + juce::String(track.lastPlayedDeck + 1)
                              : track.artist,
                          8 + columns.title, 0, columns.artist - 8, height,
                          juce::Justification::centredLeft, true);
        graphics.setColour(mutedColour.withAlpha(0.82f));
        graphics.drawText(track.album, 8 + columns.title + columns.artist,
                          0, columns.album - 8, height,
                          juce::Justification::centredLeft, true);
        auto right = width - 8;
        const auto minutes = std::max(0, static_cast<int>(track.durationSeconds) / 60);
        const auto seconds = std::max(0, static_cast<int>(track.durationSeconds) % 60);
        const auto playedTime = track.lastPlayedAt.substring(5, 16);
        graphics.drawText(showingRecentPlay
                              ? playedTime.substring(3, 5) + "/" + playedTime.substring(0, 2)
                                    + " " + playedTime.substring(6, 11)
                              : juce::String(minutes).paddedLeft('0', 2)
                                    + ":" + juce::String(seconds).paddedLeft('0', 2),
                          right - columns.length, 0, columns.length, height,
                          juce::Justification::centred, true);
        right -= columns.length;
        graphics.setColour(track.musicalKey.isNotEmpty()
                               ? juce::Colour(0xff42c8ac) : mutedColour);
        graphics.drawText(track.musicalKey.isEmpty() ? "--" : track.musicalKey,
                          right - columns.key, 0, columns.key, height,
                          juce::Justification::centred, false);
        right -= columns.key;
        graphics.setColour(track.bpm > 0.0 ? juce::Colour(0xffe6c45f) : mutedColour);
        graphics.drawText(track.bpm > 0.0 ? juce::String(track.bpm, 1) : "--",
                          right - columns.bpm, 0, columns.bpm, height,
                          juce::Justification::centred, false);
        if (rowIsSelected)
        {
            graphics.setColour(accentColour);
            graphics.fillRect(0, 5, 2, height - 10);
        }
        graphics.setColour(juce::Colour(0x24303840));
        graphics.drawHorizontalLine(height - 1, 0.0f, static_cast<float>(width));
    }

    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override
    {
        if (row < 0 || static_cast<std::size_t>(row) >= tracks.size())
            return;

        const auto track = tracks[static_cast<std::size_t>(row)];
        const auto file = juce::File(track.path);
        if (!file.existsAsFile())
        {
            status.setText("Track is missing. Relinking is not implemented yet.",
                           juce::dontSendNotification);
            return;
        }

        const auto selectedDeckId = deckBox.getSelectedId();
        const auto deckIndex = selectedDeckId == 2 ? std::size_t(1) : std::size_t(0);
        loadTrackForDeck(deckIndex, track);
    }

    void refreshTracks()
    {
        const auto selectedPlaylist = selectedPlaylistName();
        const auto showingRecentPlays = isRecentPlaysSelected();
        tableHeader.setRecentPlays(showingRecentPlays);
        scanButton.setButtonText(showingRecentPlays ? "REFRESH" : "ADD FOLDER");
        if (showingRecentPlays)
            tracks = library.searchRecentPlays(searchBox.getText());
        else
            tracks = selectedPlaylist.isEmpty()
                ? library.search(searchBox.getText())
                : library.searchPlaylist(selectedPlaylist, searchBox.getText());
        list.updateContent();
        list.repaint();
        const auto description = isRecentPlaysSelected()
            ? " recent plays" : " tracks"
                + (selectedPlaylist.isEmpty() ? juce::String {} : " in " + selectedPlaylist);
        status.setText(juce::String(tracks.size()) + description,
                       juce::dontSendNotification);
    }

    juce::String selectedPlaylistName() const
    {
        return selectedCollection >= 2
                && static_cast<std::size_t>(selectedCollection - 2) < playlists.size()
            ? playlists[static_cast<std::size_t>(selectedCollection - 2)]
            : juce::String {};
    }

    bool isRecentPlaysSelected() const
    {
        return selectedCollection == 1;
    }

    void refreshPlaylists(const juce::String& selectedName, bool selectRecentPlays = false)
    {
        playlists = library.getPlaylists();
        std::vector<juce::String> collectionNames { "All tracks", "Recent plays" };
        int nextSelected = selectRecentPlays ? 1 : 0;
        for (std::size_t index = 0; index < playlists.size(); ++index)
        {
            if (playlists[index].equalsIgnoreCase(selectedName))
                nextSelected = static_cast<int>(index + 2);
            collectionNames.push_back(playlists[index]);
        }
        selectedCollection = nextSelected;
        collections->setItems(std::move(collectionNames), selectedCollection);
        addTrackButton.setEnabled(selectedCollection >= 2);
        removeTrackButton.setEnabled(selectedCollection >= 2);
    }

    void createPlaylist()
    {
        const auto name = playlistNameBox.getText().trim();
        if (name.isEmpty())
        {
            status.setText("Enter a name for the new playlist.",
                           juce::dontSendNotification);
            return;
        }

        if (!library.createPlaylist(name))
        {
            status.setText(library.getLastError(), juce::dontSendNotification);
            return;
        }

        playlistNameBox.clear();
        refreshPlaylists(name);
        refreshTracks();
    }

    void updateSelectedTrackInPlaylist(bool shouldAdd)
    {
        const auto playlist = selectedPlaylistName();
        const auto selectedRow = list.getSelectedRow();
        if (playlist.isEmpty())
        {
            status.setText("Select a playlist first.", juce::dontSendNotification);
            return;
        }
        if (selectedRow < 0 || static_cast<std::size_t>(selectedRow) >= tracks.size())
        {
            status.setText("Select a track first.", juce::dontSendNotification);
            return;
        }

        const auto trackId = tracks[static_cast<std::size_t>(selectedRow)].id;
        const auto succeeded = shouldAdd
            ? library.addTrackToPlaylist(playlist, trackId)
            : library.removeTrackFromPlaylist(playlist, trackId);
        if (!succeeded)
        {
            status.setText(library.getLastError(), juce::dontSendNotification);
            return;
        }

        refreshTracks();
        status.setText(shouldAdd ? "Track added to " + playlist
                                 : "Track removed from " + playlist,
                       juce::dontSendNotification);
    }

    void chooseFolder()
    {
        if (library.isScanning())
            return;

        fileChooser = std::make_unique<juce::FileChooser>(
            "Choose a music folder", juce::File {}, juce::String {});
        juce::Component::SafePointer<LibraryPanel> safeThis(this);
        fileChooser->launchAsync(juce::FileBrowserComponent::openMode
                                     | juce::FileBrowserComponent::canSelectDirectories,
                                 [safeThis](const juce::FileChooser& chooser)
                                 {
                                     if (safeThis == nullptr)
                                         return;

                                     const auto directory = chooser.getResult();
                                     juce::MessageManager::callAsync(
                                         [safeThis]
                                         {
                                             if (safeThis != nullptr)
                                                 safeThis->fileChooser.reset();
                                         });
                                     if (!directory.isDirectory())
                                         return;

                                     safeThis->status.setText("Scanning folder...",
                                                              juce::dontSendNotification);
                                     const auto accepted = safeThis->library.scanDirectoryAsync(
                                         directory,
                                         {
                                             [safeThis](int indexed, int failures)
                                             {
                                                 if (safeThis != nullptr)
                                                     safeThis->status.setText(
                                                         juce::String(indexed) + " indexed, "
                                                             + juce::String(failures) + " failed",
                                                         juce::dontSendNotification);
                                             },
                                             [safeThis](juce::String error)
                                             {
                                                 if (safeThis == nullptr)
                                                     return;
                                                 safeThis->refreshTracks();
                                                 safeThis->status.setText(
                                                     error.isEmpty() ? "Scan complete" : error,
                                                     juce::dontSendNotification);
                                             }
                                         });
                                     if (!accepted && safeThis != nullptr)
                                         safeThis->status.setText(safeThis->library.getLastError(),
                                                                  juce::dontSendNotification);
                                 });
    }

    library::TrackLibrary& library;
    std::function<void(std::size_t, library::Track)> loadTrackForDeck;
    std::vector<library::Track> tracks;
    std::vector<juce::String> playlists;
    juce::Label libraryTitle;
    juce::TextEditor searchBox;
    juce::TextButton scanButton { "ADD FOLDER" };
    juce::ComboBox deckBox;
    std::unique_ptr<CollectionSidebar> collections;
    juce::TextEditor playlistNameBox;
    TrackTableHeader tableHeader;
    juce::TextButton createPlaylistButton { "CREATE" };
    juce::TextButton addTrackButton { "ADD SELECTED" };
    juce::TextButton removeTrackButton { "REMOVE" };
    juce::Label status;
    juce::ListBox list;
    std::unique_ptr<juce::FileChooser> fileChooser;
    int selectedCollection = 0;
};

}

LibraryWindow::LibraryWindow(library::TrackLibrary& trackLibrary,
                             std::function<void(std::size_t, library::Track)> loadTrack)
    : juce::DocumentWindow("Vyra Library", backgroundColour,
                           juce::DocumentWindow::allButtons, true)
{
    setUsingNativeTitleBar(true);
    setContentOwned(new LibraryPanel(trackLibrary, std::move(loadTrack)), true);
    setResizable(true, true);
    setResizeLimits(720, 440, 2200, 1400);
    centreWithSize(960, 620);
}

void LibraryWindow::closeButtonPressed()
{
    setVisible(false);
}

void LevelMeter::setLevel(float newLevel) noexcept
{
    level.store(std::clamp(newLevel, 0.0f, 1.0f), std::memory_order_relaxed);
    repaint();
}

void LevelMeter::paint(juce::Graphics& graphics)
{
    auto bounds = getLocalBounds().toFloat();
    graphics.setColour(juce::Colour(0xff0d1013));
    graphics.fillRoundedRectangle(bounds, 3.0f);

    const auto value = level.load(std::memory_order_relaxed);
    const auto filledHeight = bounds.getHeight() * value;
    auto active = bounds.withY(bounds.getBottom() - filledHeight).withHeight(filledHeight).reduced(2.0f, 0.0f);
    graphics.setColour(value > 0.9f ? juce::Colour(0xffff6b63) : accentColour);
    graphics.fillRoundedRectangle(active, 2.0f);

    graphics.setColour(borderColour);
    graphics.drawRoundedRectangle(bounds, 3.0f, 1.0f);
}

DeckComponent::DeckComponent(audio::AudioEngine& audioEngine,
                             library::TrackLibrary& trackLibrary,
                             std::size_t index)
    : engine(audioEngine), library(trackLibrary), deckIndex(index)
{
    deckTitle.setColour(juce::Label::textColourId, accentColour);
    deckTitle.setFont(juce::Font(11.0f, juce::Font::bold));
    deckTitle.setText("DECK " + juce::String(static_cast<int>(deckIndex + 1))
                          + "  ·  BPM --",
                      juce::dontSendNotification);
    addAndMakeVisible(deckTitle);

    trackTitle.setText("No track loaded", juce::dontSendNotification);
    trackTitle.setColour(juce::Label::textColourId, juce::Colour(0xffedf2f4));
    trackTitle.setFont(juce::Font(16.0f, juce::Font::bold));
    trackTitle.setMinimumHorizontalScale(0.75f);
    addAndMakeVisible(trackTitle);

    timeLabel.setText("00:00 / 00:00", juce::dontSendNotification);
    timeLabel.setColour(juce::Label::textColourId, mutedColour);
    timeLabel.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), "Regular", 12.0f));
    addAndMakeVisible(timeLabel);

    styleButton(loadButton, juce::Colour(0xff29323a));
    styleButton(playButton, panelColour);
    styleButton(cueButton, juce::Colour(0xff29323a));
    pflButton.setClickingTogglesState(true);
    pflButton.setTooltip("Pre-fader listen on the separate headphone output.");
    styleButton(pflButton, juce::Colour(0xff29323a));
    addAndMakeVisible(loadButton);
    addAndMakeVisible(playButton);
    addAndMakeVisible(cueButton);
    addAndMakeVisible(pflButton);
    loadButton.onClick = [this] { openTrackChooser(); };
    playButton.onClick = [this] { engine.togglePlay(deckIndex); };
    cueButton.onClick = [this] { engine.cue(deckIndex); };
    pflButton.onClick = [this]
    {
        engine.setHeadphoneCue(deckIndex, pflButton.getToggleState());
    };

    for (std::size_t index = 0; index < hotCueButtons.size(); ++index)
    {
        auto& button = hotCueButtons[index];
        button.setButtonText(juce::String(static_cast<int>(index + 1)) + " SET");
        button.setTooltip("Click to set/recall this hot cue; Shift-click to clear it.");
        styleButton(button, juce::Colour(0xff29323a));
        button.onClick = [this, index]
        {
            if (juce::ModifierKeys::getCurrentModifiers().isShiftDown())
            {
                hotCuePositions[index].reset();
                refreshHotCueButton(index);
                savePerformanceState();
                return;
            }

            if (hotCuePositions[index].has_value())
            {
                engine.setPosition(deckIndex, *hotCuePositions[index]);
                return;
            }

            if (engine.getLength(deckIndex) <= 0.0)
                return;

            const auto position = engine.getPosition(deckIndex);
            hotCuePositions[index] = position;
            refreshHotCueButton(index);
            savePerformanceState();
        };
        addAndMakeVisible(button);
    }

    beatJumpButtons[0].setButtonText("-4 BEAT");
    beatJumpButtons[1].setButtonText("+4 BEAT");
    for (std::size_t index = 0; index < beatJumpButtons.size(); ++index)
    {
        auto& button = beatJumpButtons[index];
        button.setTooltip("Jump by four beats using the analyzed track BPM.");
        styleButton(button, juce::Colour(0xff29323a));
        button.onClick = [this, index]
        {
            engine.jumpBeats(deckIndex, index == 0 ? -4 : 4);
        };
        addAndMakeVisible(button);
    }

    loopBeatsBox.addItem("1 beat", 1);
    loopBeatsBox.addItem("2 beats", 2);
    loopBeatsBox.addItem("4 beats", 4);
    loopBeatsBox.addItem("8 beats", 8);
    loopBeatsBox.setSelectedId(3, juce::dontSendNotification);
    loopBeatsBox.setColour(juce::ComboBox::backgroundColourId, panelColour);
    loopBeatsBox.setColour(juce::ComboBox::textColourId, juce::Colour(0xffedf2f4));
    loopBeatsBox.setColour(juce::ComboBox::outlineColourId, borderColour);
    loopButton.setClickingTogglesState(true);
    styleButton(loopButton, juce::Colour(0xff29323a));
    loopButton.onClick = [this]
    {
        if (loopButton.getToggleState())
        {
            if (!engine.setLoop(deckIndex, loopBeatsBox.getSelectedId()))
                loopButton.setToggleState(false, juce::dontSendNotification);
        }
        else
        {
            engine.clearLoop(deckIndex);
        }
        savePerformanceState();
    };
    addAndMakeVisible(loopButton);
    addAndMakeVisible(loopBeatsBox);
    loopBeatsBox.onChange = [this] { savePerformanceState(); };

    styleButton(syncButton, juce::Colour(0xff29323a));
    syncButton.setTooltip("Match this deck's playback speed to the other deck's analyzed BPM.");
    syncButton.onClick = [this]
    {
        const auto otherDeck = deckIndex == 0 ? 1U : 0U;
        if (engine.syncDeckToDeck(deckIndex, otherDeck))
        {
            pitchSlider.setValue(engine.getDeckPitch(deckIndex), juce::sendNotificationSync);
            syncButton.setTooltip("Deck tempo matched to the other deck (pitch changes too).");
        }
        else
            syncButton.setTooltip("Load tracks with an estimated BPM on both decks first.");
    };
    addAndMakeVisible(syncButton);

    pitchLabel.setColour(juce::Label::textColourId, mutedColour);
    pitchLabel.setFont(juce::Font(10.0f, juce::Font::bold));
    pitchLabel.setJustificationType(juce::Justification::centred);
    pitchSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    pitchSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    pitchSlider.setRange(-16.0, 16.0, 0.1);
    pitchSlider.setValue(0.0, juce::dontSendNotification);
    pitchSlider.setColour(juce::Slider::trackColourId, accentColour);
    pitchSlider.setColour(juce::Slider::backgroundColourId, juce::Colour(0xff303840));
    pitchSlider.setTooltip("Pitch/speed control; key lock is not enabled.");
    pitchSlider.onValueChange = [this]
    {
        engine.setDeckPitch(deckIndex, static_cast<float>(pitchSlider.getValue()));
        pitchLabel.setText("PITCH " + juce::String(pitchSlider.getValue(), 1) + "%",
                           juce::dontSendNotification);
    };
    addAndMakeVisible(pitchSlider);
    addAndMakeVisible(pitchLabel);

    positionSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    positionSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    positionSlider.setRange(0.0, 0.0, 0.01);
    positionSlider.setColour(juce::Slider::trackColourId, accentColour);
    positionSlider.setColour(juce::Slider::backgroundColourId, juce::Colour(0xff303840));
    addAndMakeVisible(positionSlider);
    positionSlider.onValueChange = [this]
    {
        if (!updatingPosition)
            engine.setPosition(deckIndex, positionSlider.getValue());
    };

    gainLabel.setColour(juce::Label::textColourId, mutedColour);
    gainLabel.setFont(juce::Font(10.0f, juce::Font::bold));
    addAndMakeVisible(gainLabel);

    gainSlider.setSliderStyle(juce::Slider::LinearVertical);
    gainSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    gainSlider.setRange(0.0, 1.0, 0.001);
    gainSlider.setValue(1.0, juce::dontSendNotification);
    gainSlider.setColour(juce::Slider::trackColourId, accentColour);
    gainSlider.setColour(juce::Slider::backgroundColourId, juce::Colour(0xff303840));
    gainSlider.onValueChange = [this]
    {
        engine.setDeckGain(deckIndex, static_cast<float>(gainSlider.getValue()));
    };
    addAndMakeVisible(gainSlider);
    addAndMakeVisible(meter);

    const std::array<juce::String, 3> eqNames { "LOW", "MID", "HIGH" };
    for (std::size_t index = 0; index < eqSliders.size(); ++index)
    {
        eqLabels[index].setText(eqNames[index], juce::dontSendNotification);
        eqLabels[index].setColour(juce::Label::textColourId, mutedColour);
        eqLabels[index].setFont(juce::Font(10.0f, juce::Font::bold));
        eqLabels[index].setJustificationType(juce::Justification::centred);
        addAndMakeVisible(eqLabels[index]);

        auto& slider = eqSliders[index];
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setRange(-12.0, 12.0, 0.1);
        slider.setValue(0.0, juce::dontSendNotification);
        slider.setColour(juce::Slider::rotarySliderFillColourId, accentColour);
        slider.setColour(juce::Slider::rotarySliderOutlineColourId, borderColour);
        slider.onValueChange = [this]
        {
            engine.setDeckEq(deckIndex,
                             static_cast<float>(eqSliders[0].getValue()),
                             static_cast<float>(eqSliders[1].getValue()),
                             static_cast<float>(eqSliders[2].getValue()));
        };
        addAndMakeVisible(slider);
    }

    filterLabel.setColour(juce::Label::textColourId, mutedColour);
    filterLabel.setFont(juce::Font(10.0f, juce::Font::bold));
    filterLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(filterLabel);
    filterSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    filterSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    filterSlider.setRange(-1.0, 1.0, 0.01);
    filterSlider.setValue(0.0, juce::dontSendNotification);
    filterSlider.setColour(juce::Slider::rotarySliderFillColourId, accentColour);
    filterSlider.setColour(juce::Slider::rotarySliderOutlineColourId, borderColour);
    filterSlider.onValueChange = [this]
    {
        engine.setDeckFilter(deckIndex, static_cast<float>(filterSlider.getValue()));
    };
    addAndMakeVisible(filterSlider);

    echoLabel.setColour(juce::Label::textColourId, mutedColour);
    echoLabel.setFont(juce::Font(10.0f, juce::Font::bold));
    echoLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(echoLabel);
    echoSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    echoSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    echoSlider.setRange(0.0, 0.8, 0.01);
    echoSlider.setValue(0.0, juce::dontSendNotification);
    echoSlider.setColour(juce::Slider::rotarySliderFillColourId, accentColour);
    echoSlider.setColour(juce::Slider::rotarySliderOutlineColourId, borderColour);
    echoSlider.onValueChange = [this]
    {
        engine.setDeckEcho(deckIndex, static_cast<float>(echoSlider.getValue()));
    };
    addAndMakeVisible(echoSlider);
    startTimerHz(24);
}

DeckComponent::~DeckComponent()
{
    stopTimer();
}

void DeckComponent::paint(juce::Graphics& graphics)
{
    auto bounds = getLocalBounds().toFloat();
    graphics.setColour(juce::Colour(0xff171a1c));
    graphics.fillRect(bounds);
    graphics.setColour(juce::Colour(0x8034383a));
    graphics.drawRect(bounds.reduced(0.5f), 0.7f);
    graphics.setColour(deckIndex == 0 ? juce::Colour(0xff29a7dc)
                                      : juce::Colour(0xff39c78c));
    graphics.fillRect(bounds.getX() + 1.0f, bounds.getY() + 1.0f,
                      bounds.getWidth() - 2.0f, 1.5f);

    auto content = getLocalBounds().reduced(12);
    content.removeFromTop(20 + 4 + 24 + 4);
    auto waveform = content.removeFromTop(std::min(36, content.getHeight()));
    graphics.setColour(juce::Colour(0xff101416));
    graphics.fillRect(waveform);
    graphics.setColour(juce::Colour(0xff202629));
    graphics.drawHorizontalLine(waveform.getCentreY(),
                                static_cast<float>(waveform.getX() + 4),
                                static_cast<float>(waveform.getRight() - 4));
    if (this->waveform.empty())
    {
        graphics.setColour(mutedColour);
        graphics.setFont(juce::Font(11.0f));
        graphics.drawText("WAVEFORM UNAVAILABLE", waveform,
                          juce::Justification::centred, true);
    }
    else
    {
        const auto centreY = waveform.getCentreY();
        const auto maxHalfHeight = std::max(1.0f, waveform.getHeight() * 0.34f);
        graphics.setColour(deckIndex == 0 ? juce::Colour(0xd929a7dc)
                                          : juce::Colour(0xd939c78c));
        for (std::size_t index = 0; index < this->waveform.size(); ++index)
        {
            const auto x = static_cast<float>(waveform.getX() + 4)
                + static_cast<float>(index) * static_cast<float>(waveform.getWidth() - 8)
                    / static_cast<float>(this->waveform.size());
            const auto amplitude = maxHalfHeight
                * static_cast<float>(this->waveform[index]) / 255.0f;
            graphics.drawVerticalLine(static_cast<int>(x), centreY - amplitude,
                                      centreY + amplitude);
        }
    }

    const auto length = engine.getLength(deckIndex);
    if (length > 0.0)
    {
        const std::array<juce::Colour, 3> cueColours {
            juce::Colour(0xffffb454), juce::Colour(0xff62b5ff), juce::Colour(0xffff6b9a)
        };
        for (std::size_t index = 0; index < hotCuePositions.size(); ++index)
        {
            if (!hotCuePositions[index].has_value())
                continue;

            const auto cueProgress = static_cast<float>(
                std::clamp(*hotCuePositions[index] / length, 0.0, 1.0));
            const auto cueX = static_cast<float>(waveform.getX() + 4)
                + cueProgress * static_cast<float>(waveform.getWidth() - 8);
            graphics.setColour(cueColours[index]);
            graphics.drawVerticalLine(static_cast<int>(cueX), waveform.getY() + 3.0f,
                                      waveform.getBottom() - 3.0f);
        }

        const auto progress = static_cast<float>(
            std::clamp(engine.getPosition(deckIndex) / length, 0.0, 1.0));
        const auto x = static_cast<float>(waveform.getX() + 4)
            + progress * static_cast<float>(waveform.getWidth() - 8);
        graphics.setColour(juce::Colour(0xfff1f5f6));
        graphics.drawVerticalLine(static_cast<int>(x), waveform.getY() + 3.0f,
                                  waveform.getBottom() - 3.0f);
    }
}

void DeckComponent::resized()
{
    auto area = getLocalBounds().reduced(12);
    deckTitle.setBounds(area.removeFromTop(20));
    area.removeFromTop(4);
    auto trackRow = area.removeFromTop(24);
    trackTitle.setBounds(trackRow.removeFromLeft(std::max(40, trackRow.getWidth() - 122)));
    loadButton.setBounds(trackRow);
    area.removeFromTop(4);
    area.removeFromTop(std::min(36, area.getHeight()));
    area.removeFromTop(2);
    timeLabel.setBounds(area.removeFromTop(18));
    positionSlider.setBounds(area.removeFromTop(18));
    area.removeFromTop(4);
    auto hotCueRow = area.removeFromTop(26);
    for (std::size_t index = 0; index < hotCueButtons.size(); ++index)
    {
        auto cueArea = hotCueRow.removeFromLeft(std::min(55, hotCueRow.getWidth() / 3));
        hotCueButtons[index].setBounds(cueArea);
        hotCueRow.removeFromLeft(6);
    }
    loopButton.setBounds(hotCueRow.removeFromLeft(48));
    hotCueRow.removeFromLeft(4);
    loopBeatsBox.setBounds(hotCueRow.removeFromLeft(58));
    hotCueRow.removeFromLeft(4);
    for (auto& button : beatJumpButtons)
    {
        const auto buttonWidth = std::min(52, hotCueRow.getWidth() / 2);
        button.setBounds(hotCueRow.removeFromLeft(buttonWidth));
        hotCueRow.removeFromLeft(3);
    }
    syncButton.setBounds(hotCueRow.removeFromRight(std::min(42, hotCueRow.getWidth())));
    area.removeFromTop(4);

    auto controls = area.removeFromTop(34);
    const auto controlsTop = controls.getY();
    playButton.setBounds(controls.removeFromLeft(100));
    controls.removeFromLeft(8);
    cueButton.setBounds(controls.removeFromLeft(76));
    controls.removeFromLeft(6);
    pflButton.setBounds(controls.removeFromLeft(54));
    pflButton.setEnabled(engine.isHeadphoneMonitoringEnabled());
    if (!pflButton.isEnabled())
        pflButton.setToggleState(false, juce::dontSendNotification);
    controls.removeFromLeft(6);
    auto pitchArea = controls.removeFromLeft(std::min(118, controls.getWidth()));
    pitchLabel.setBounds(pitchArea.removeFromTop(16));
    pitchSlider.setBounds(pitchArea.removeFromTop(22));
    controls.removeFromLeft(8);

    const auto meterWidth = 12;
    meter.setBounds(controls.removeFromRight(meterWidth));
    controls.removeFromRight(8);

    gainLabel.setBounds(controls.removeFromRight(56));
    gainSlider.setBounds(controls.removeFromRight(28)
                             .withY(controlsTop - 7).withHeight(50));

    area.removeFromTop(2);
    auto dspArea = area;
    const auto knobWidth = std::min(52, dspArea.getWidth() / 5);
    const auto knobHeight = std::min(48, dspArea.getHeight());
    for (std::size_t index = 0; index < eqSliders.size(); ++index)
    {
        auto knobColumn = dspArea.removeFromLeft(knobWidth);
        eqLabels[index].setBounds(knobColumn.removeFromTop(12));
        eqSliders[index].setBounds(knobColumn.removeFromTop(knobHeight));
        dspArea.removeFromLeft(4);
    }

    auto filterColumn = dspArea.removeFromLeft(knobWidth);
    filterLabel.setBounds(filterColumn.removeFromTop(12));
    filterSlider.setBounds(filterColumn.removeFromTop(knobHeight));
    dspArea.removeFromLeft(4);

    auto echoColumn = dspArea.removeFromLeft(knobWidth);
    echoLabel.setBounds(echoColumn.removeFromTop(12));
    echoSlider.setBounds(echoColumn.removeFromTop(knobHeight));
}

void DeckComponent::mouseDown(const juce::MouseEvent& event)
{
    auto waveformBounds = getLocalBounds().reduced(12);
    waveformBounds.removeFromTop(20 + 4 + 24 + 4);
    waveformBounds = waveformBounds.removeFromTop(
        std::min(36, waveformBounds.getHeight()));
    if (!waveformBounds.contains(event.getPosition()))
        return;

    const auto length = engine.getLength(deckIndex);
    if (length <= 0.0)
        return;

    const auto waveformWidth = std::max(1, waveformBounds.getWidth() - 8);
    const auto relativeX = std::clamp(event.x - waveformBounds.getX() - 4,
                                      0, waveformWidth);
    const auto position = length * static_cast<double>(relativeX)
        / static_cast<double>(waveformWidth);
    engine.setPosition(deckIndex, position);
}

void DeckComponent::timerCallback()
{
    const auto length = engine.getLength(deckIndex);
    const auto position = engine.getPosition(deckIndex);

    if (std::abs(length - sliderLength) > 0.001)
    {
        updatingPosition = true;
        positionSlider.setRange(0.0, length, 0.01);
        sliderLength = length;
        updatingPosition = false;
    }

    if (!positionSlider.isMouseButtonDown())
    {
        updatingPosition = true;
        positionSlider.setValue(position, juce::dontSendNotification);
        updatingPosition = false;
    }

    timeLabel.setText(formatTime(position) + " / " + formatTime(length), juce::dontSendNotification);
    const auto playing = engine.isPlaying(deckIndex);
    const auto transportText = playing ? juce::String("PAUSE")
                                       : juce::String("PLAY");
    if (playButton.getButtonText() != transportText)
    {
        playButton.setButtonText(transportText);
        styleButton(playButton, playing
                                    ? (deckIndex == 0 ? juce::Colour(0xff155976)
                                                      : juce::Colour(0xff1a6650))
                                    : panelColour);
    }
    pflButton.setEnabled(engine.isHeadphoneMonitoringEnabled());
    pflButton.setToggleState(engine.isHeadphoneCueEnabled(deckIndex),
                             juce::dontSendNotification);
    meter.setLevel(engine.getDeckLevel(deckIndex));
    repaint();
}

void DeckComponent::openTrackChooser()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Load an audio track", juce::File {}, engine.getSupportedFileWildcard());
    juce::Component::SafePointer<DeckComponent> safeThis(this);
    fileChooser->launchAsync(juce::FileBrowserComponent::openMode
                                 | juce::FileBrowserComponent::canSelectFiles,
                             [safeThis](const juce::FileChooser& chooser)
                             {
                                 if (safeThis == nullptr)
                                     return;

                                 const auto selectedFile = chooser.getResult();
                                 juce::MessageManager::callAsync(
                                     [safeThis]
                                     {
                                         if (safeThis != nullptr)
                                             safeThis->fileChooser.reset();
                                     });
                                 if (!selectedFile.existsAsFile())
                                     return;

                                 const auto title = selectedFile.getFileName();
                                 safeThis->loadFile(selectedFile);
                             });
}

void DeckComponent::loadFile(juce::File file, std::vector<std::uint8_t> cachedWaveform,
                             juce::String displayTitle)
{
    const auto loadedPath = file.getFullPathName();
    const auto title = displayTitle.isNotEmpty()
        ? std::move(displayTitle) : file.getFileName();
    const auto previousTitle = trackTitle.getText();
    trackTitle.setText("Loading " + title, juce::dontSendNotification);
    juce::Component::SafePointer<DeckComponent> safeThis(this);
    engine.loadTrackAsync(deckIndex, std::move(file),
                          [safeThis, title, loadedPath, previousTitle](juce::String error,
                                                           std::vector<std::uint8_t> newWaveform,
                                                           double bpm,
                                                           juce::String musicalKey)
                          {
                              if (safeThis == nullptr)
                                  return;

                              if (error.isNotEmpty())
                              {
                                  safeThis->trackTitle.setText(previousTitle,
                                                               juce::dontSendNotification);
                                  juce::AlertWindow::showMessageBoxAsync(
                                      juce::MessageBoxIconType::WarningIcon,
                                      "Could not load track", error);
                              }
                              else
                              {
                                  safeThis->trackPath = loadedPath;
                                  ++safeThis->loadRevision;
                                  safeThis->deckTitle.setText(
                                      "DECK "
                                          + juce::String(static_cast<int>(
                                                safeThis->deckIndex + 1))
                                          + "  ·  BPM "
                                          + (bpm > 0.0
                                                 ? juce::String(bpm, 1)
                                                 : juce::String("--"))
                                          + (musicalKey.isNotEmpty()
                                                 ? "  ·  " + musicalKey
                                                 : juce::String {}),
                                      juce::dontSendNotification);
                                  safeThis->trackTitle.setText(title,
                                                               juce::dontSendNotification);
                                  safeThis->waveform = std::move(newWaveform);
                                  library::TrackPerformanceState performanceState;
                                  if (!safeThis->library.loadPerformanceState(
                                          loadedPath, performanceState))
                                  {
                                      juce::AlertWindow::showMessageBoxAsync(
                                          juce::MessageBoxIconType::WarningIcon,
                                          "Could not load performance settings",
                                          safeThis->library.getLastError());
                                      performanceState = {};
                                  }
                                  for (std::size_t index = 0;
                                       index < safeThis->hotCuePositions.size(); ++index)
                                  {
                                      const auto position =
                                          performanceState.hotCueSeconds[index];
                                      if (position >= 0.0
                                          && position <= safeThis->engine.getLength(
                                              safeThis->deckIndex))
                                          safeThis->hotCuePositions[index] = position;
                                      else
                                          safeThis->hotCuePositions[index].reset();
                                      safeThis->refreshHotCueButton(index);
                                  }
                                  safeThis->loopBeatsBox.setSelectedId(
                                      performanceState.loopBeats,
                                      juce::dontSendNotification);
                                  safeThis->loopButton.setToggleState(
                                      false, juce::dontSendNotification);
                                  safeThis->repaint();
                              }
                          }, std::move(cachedWaveform));
}

void DeckComponent::refreshHotCueButton(std::size_t cueIndex)
{
    if (cueIndex >= hotCueButtons.size())
        return;
    auto& button = hotCueButtons[cueIndex];
    if (hotCuePositions[cueIndex].has_value())
    {
        button.setButtonText(juce::String(static_cast<int>(cueIndex + 1)) + " "
                             + formatTime(*hotCuePositions[cueIndex]));
        button.setColour(juce::TextButton::buttonColourId,
                         juce::Colour(0xff176c5a));
    }
    else
    {
        button.setButtonText(juce::String(static_cast<int>(cueIndex + 1)) + " SET");
        button.setColour(juce::TextButton::buttonColourId,
                         juce::Colour(0xff29323a));
    }
}

void DeckComponent::savePerformanceState()
{
    if (trackPath.isEmpty())
        return;
    library::TrackPerformanceState state;
    for (std::size_t index = 0; index < hotCuePositions.size(); ++index)
    {
        if (hotCuePositions[index].has_value())
            state.hotCueSeconds[index] = *hotCuePositions[index];
    }
    const auto selectedLoop = loopBeatsBox.getSelectedId();
    state.loopBeats = selectedLoop == 1 || selectedLoop == 2
        || selectedLoop == 4 || selectedLoop == 8 ? selectedLoop : 4;
    if (!library.savePerformanceState(trackPath, state))
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "Could not save performance settings", library.getLastError());
    }
}

juce::String DeckComponent::getTrackTitle() const
{
    const auto title = trackTitle.getText();
    return title.startsWith("Loading ") || title == "No track loaded"
        ? juce::String {} : title;
}

juce::String DeckComponent::getTrackPath() const
{
    return trackPath;
}

std::uint64_t DeckComponent::getLoadRevision() const noexcept
{
    return loadRevision;
}

bool DeckComponent::hasLoadedTrack() const
{
    return engine.getLength(deckIndex) > 0.0;
}

void DeckComponent::triggerLoad()
{
    openTrackChooser();
}

void DeckComponent::seekToNormalized(float position)
{
    const auto length = engine.getLength(deckIndex);
    if (length <= 0.0)
        return;
    engine.setPosition(deckIndex,
                       length * static_cast<double>(std::clamp(position, 0.0f, 1.0f)));
}

void DeckComponent::triggerPlay()
{
    playButton.triggerClick();
}

void DeckComponent::triggerCue()
{
    cueButton.triggerClick();
}

void DeckComponent::triggerHotCue(std::size_t cueIndex)
{
    if (cueIndex < hotCueButtons.size())
        hotCueButtons[cueIndex].triggerClick();
}

void DeckComponent::triggerLoop()
{
    loopButton.triggerClick();
}

void DeckComponent::triggerBeatJump(bool forward)
{
    beatJumpButtons[forward ? 1U : 0U].triggerClick();
}

void DeckComponent::triggerSync()
{
    syncButton.triggerClick();
}

void DeckComponent::triggerHeadphoneCue()
{
    if (pflButton.isEnabled())
        pflButton.triggerClick();
}

void DeckComponent::triggerJog(float relativeSteps)
{
    const auto position = engine.getPosition(deckIndex) + relativeSteps * 0.02;
    engine.setPosition(deckIndex, position);
}

void DeckComponent::setGainFromController(float value)
{
    gainSlider.setValue(std::clamp(value, 0.0f, 1.0f), juce::sendNotificationSync);
}

void DeckComponent::setPitchFromController(float value)
{
    pitchSlider.setValue(
        integrations::MidiController::pitchPercentFromMidi(value),
        juce::sendNotificationSync);
}

void DeckComponent::setEqFromController(std::size_t band, float value)
{
    if (band < eqSliders.size())
        eqSliders[band].setValue(
            std::clamp(value, 0.0f, 1.0f) * 24.0f - 12.0,
            juce::sendNotificationSync);
}

void DeckComponent::setFilterFromController(float value)
{
    filterSlider.setValue(std::clamp(value, 0.0f, 1.0f) * 2.0f - 1.0f,
                          juce::sendNotificationSync);
}

void DeckComponent::setEchoFromController(float value)
{
    echoSlider.setValue(std::clamp(value, 0.0f, 1.0f),
                        juce::sendNotificationSync);
}

void DeckComponent::paintMiniOverview(juce::Graphics& graphics,
                                      juce::Rectangle<float> bounds) const
{
    const auto length = engine.getLength(deckIndex);
    graphics.setColour(juce::Colour(0xff111518));
    graphics.fillRect(bounds);
    graphics.setColour(juce::Colour(0xff262d30));
    graphics.drawHorizontalLine(static_cast<int>(bounds.getCentreY()),
                                bounds.getX() + 3.0f, bounds.getRight() - 3.0f);
    if (waveform.empty())
    {
        graphics.setColour(length > 0.0 ? juce::Colour(0xff68737a) : mutedColour);
        graphics.setFont(juce::Font(9.0f));
        const auto message = length > 0.0
            ? juce::String("TRACK LOADED · WAVEFORM UNAVAILABLE")
            : juce::String("NO TRACK · CLICK TO LOAD");
        graphics.drawText(message, bounds.toNearestInt(),
                          juce::Justification::centred, true);
    }

    if (!waveform.empty())
    {
        const auto halfHeight = std::max(1.0f, bounds.getHeight() * 0.31f);
        graphics.setColour(deckIndex == 0 ? juce::Colour(0xd929a7dc)
                                          : juce::Colour(0xd939c78c));
        for (std::size_t index = 0; index < waveform.size(); ++index)
        {
            const auto x = bounds.getX() + 2.0f
                + static_cast<float>(index) * (bounds.getWidth() - 4.0f)
                    / static_cast<float>(waveform.size());
            const auto amplitude = halfHeight
                * static_cast<float>(waveform[index]) / 255.0f;
            graphics.drawVerticalLine(static_cast<int>(x),
                                      bounds.getCentreY() - amplitude,
                                      bounds.getCentreY() + amplitude);
        }
    }

    if (length > 0.0)
    {
        const auto position = std::clamp(engine.getPosition(deckIndex) / length,
                                         0.0, 1.0);
        const auto x = bounds.getX() + 2.0f
            + static_cast<float>(position) * (bounds.getWidth() - 4.0f);
        graphics.setColour(juce::Colour(0xffdce7ea));
        graphics.drawVerticalLine(static_cast<int>(x), bounds.getY() + 1.0f,
                                  bounds.getBottom() - 1.0f);
    }
}

MainComponent::MainComponent(audio::AudioEngine& audioEngine,
                             library::TrackLibrary& trackLibrary,
                             integrations::DiscordPresence& activity)
    : engine(audioEngine), library(trackLibrary), discordPresence(activity),
      leftDeck(engine, library, 0), rightDeck(engine, library, 1)
{
    lookAndFeel.setColour(juce::ResizableWindow::backgroundColourId, backgroundColour);
    lookAndFeel.setColour(juce::Slider::thumbColourId, accentColour);
    lookAndFeel.setColour(juce::Slider::trackColourId, accentColour);
    lookAndFeel.setColour(juce::Slider::backgroundColourId, juce::Colour(0xff303840));
#if JUCE_WINDOWS
    lookAndFeel.setDefaultSansSerifTypefaceName("Segoe UI");
#endif
    setLookAndFeel(&lookAndFeel);

    wordmark.setText("VYRA", juce::dontSendNotification);
    wordmark.setColour(juce::Label::textColourId, juce::Colour(0xffedf2f4));
    wordmark.setFont(juce::Font(24.0f, juce::Font::plain));
    addAndMakeVisible(wordmark);

    statusLabel.setText("DUAL-DECK PERFORMANCE", juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, mutedColour);
    statusLabel.setFont(juce::Font(11.0f, juce::Font::plain));
    addAndMakeVisible(statusLabel);

    styleButton(audioSettingsButton, juce::Colour(0xff29323a));
    audioSettingsButton.onClick = [this] { openAudioSettings(); };
    addAndMakeVisible(audioSettingsButton);

    styleButton(recordButton, juce::Colour(0xff7c302f));
    recordButton.onClick = [this]
    {
        if (engine.isRecording())
        {
            engine.stopRecording();
            recordButton.setButtonText("REC WAV");
            pauseRecordingButton.setEnabled(false);
            pauseRecordingButton.setToggleState(false, juce::dontSendNotification);
            return;
        }

        recordingChooser = std::make_unique<juce::FileChooser>(
            "Record mix as WAV", juce::File::getSpecialLocation(
                juce::File::userMusicDirectory).getChildFile("Vyra Mix.wav"),
            "*.wav");
        juce::Component::SafePointer<MainComponent> safeThis(this);
        recordingChooser->launchAsync(juce::FileBrowserComponent::saveMode
                                          | juce::FileBrowserComponent::canSelectFiles
                                          | juce::FileBrowserComponent::warnAboutOverwriting,
                                      [safeThis](const juce::FileChooser& chooser)
                                      {
                                          if (safeThis == nullptr)
                                              return;
                                          const auto file = chooser.getResult();
                                          if (file == juce::File {})
                                              return;
                                          auto destination = file;
                                          if (destination.getFileExtension().isEmpty())
                                              destination = destination.withFileExtension(".wav");
                                          const auto error = safeThis->engine.startRecording(destination);
                                          if (error.isNotEmpty())
                                          {
                                              safeThis->statusLabel.setText(
                                                  error, juce::dontSendNotification);
                                              return;
                                          }
                                          safeThis->recordButton.setButtonText("STOP REC");
                                          safeThis->pauseRecordingButton.setEnabled(true);
                                      });
    };
    addAndMakeVisible(recordButton);

    styleButton(pauseRecordingButton, juce::Colour(0xff29323a));
    pauseRecordingButton.setEnabled(false);
    pauseRecordingButton.setClickingTogglesState(true);
    pauseRecordingButton.onClick = [this]
    {
        engine.setRecordingPaused(pauseRecordingButton.getToggleState());
        pauseRecordingButton.setButtonText(
            pauseRecordingButton.getToggleState() ? "RESUME" : "PAUSE");
    };
    addAndMakeVisible(pauseRecordingButton);

    styleButton(libraryButton, juce::Colour(0xff29323a));
    libraryButton.onClick = [this] { openLibrary(); };
    addAndMakeVisible(libraryButton);

    libraryPanel = std::make_unique<LibraryPanel>(
        library,
        [this](std::size_t deckIndex, library::Track track)
        {
            auto title = track.title;
            if (track.artist.isNotEmpty())
                title = track.artist + " — " + title;
            (deckIndex == 0 ? leftDeck : rightDeck).loadFile(
                juce::File(track.path), std::move(track.waveform), std::move(title));
        });
    addAndMakeVisible(libraryPanel.get());

    addAndMakeVisible(leftDeck);
    addAndMakeVisible(rightDeck);

    crossfaderLabel.setColour(juce::Label::textColourId, mutedColour);
    crossfaderLabel.setFont(juce::Font(10.0f, juce::Font::bold));
    crossfaderLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(crossfaderLabel);
    crossfader.setSliderStyle(juce::Slider::LinearHorizontal);
    crossfader.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    crossfader.setRange(0.0, 1.0, 0.001);
    crossfader.setValue(0.5, juce::dontSendNotification);
    crossfader.onValueChange = [this]
    {
        engine.setCrossfade(static_cast<float>(crossfader.getValue()));
    };
    addAndMakeVisible(crossfader);

    masterLabel.setColour(juce::Label::textColourId, mutedColour);
    masterLabel.setFont(juce::Font(10.0f, juce::Font::bold));
    masterLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(masterLabel);
    masterGain.setSliderStyle(juce::Slider::LinearHorizontal);
    masterGain.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    masterGain.setRange(0.0, 1.0, 0.001);
    masterGain.setValue(1.0, juce::dontSendNotification);
    masterGain.onValueChange = [this]
    {
        engine.setMasterGain(static_cast<float>(masterGain.getValue()));
    };
    addAndMakeVisible(masterGain);
    addAndMakeVisible(masterMeter);

    midiController.setActionHandler([this](integrations::MidiController::Action action,
                                           float value)
    {
        using Action = integrations::MidiController::Action;
        switch (action)
        {
            case Action::playDeck1: leftDeck.triggerPlay(); break;
            case Action::cueDeck1: leftDeck.triggerCue(); break;
            case Action::hotCue1Deck1: leftDeck.triggerHotCue(0); break;
            case Action::hotCue2Deck1: leftDeck.triggerHotCue(1); break;
            case Action::hotCue3Deck1: leftDeck.triggerHotCue(2); break;
            case Action::playDeck2: rightDeck.triggerPlay(); break;
            case Action::cueDeck2: rightDeck.triggerCue(); break;
            case Action::hotCue1Deck2: rightDeck.triggerHotCue(0); break;
            case Action::hotCue2Deck2: rightDeck.triggerHotCue(1); break;
            case Action::hotCue3Deck2: rightDeck.triggerHotCue(2); break;
            case Action::crossfader:
                crossfader.setValue(value, juce::sendNotificationSync);
                break;
            case Action::gainDeck1: leftDeck.setGainFromController(value); break;
            case Action::gainDeck2: rightDeck.setGainFromController(value); break;
            case Action::loopDeck1: leftDeck.triggerLoop(); break;
            case Action::loopDeck2: rightDeck.triggerLoop(); break;
            case Action::beatJumpBackDeck1: leftDeck.triggerBeatJump(false); break;
            case Action::beatJumpForwardDeck1: leftDeck.triggerBeatJump(true); break;
            case Action::beatJumpBackDeck2: rightDeck.triggerBeatJump(false); break;
            case Action::beatJumpForwardDeck2: rightDeck.triggerBeatJump(true); break;
            case Action::syncDeck1: leftDeck.triggerSync(); break;
            case Action::syncDeck2: rightDeck.triggerSync(); break;
            case Action::headphoneCueDeck1: leftDeck.triggerHeadphoneCue(); break;
            case Action::headphoneCueDeck2: rightDeck.triggerHeadphoneCue(); break;
            case Action::pitchDeck1: leftDeck.setPitchFromController(value); break;
            case Action::pitchDeck2: rightDeck.setPitchFromController(value); break;
            case Action::lowEqDeck1: leftDeck.setEqFromController(0, value); break;
            case Action::midEqDeck1: leftDeck.setEqFromController(1, value); break;
            case Action::highEqDeck1: leftDeck.setEqFromController(2, value); break;
            case Action::lowEqDeck2: rightDeck.setEqFromController(0, value); break;
            case Action::midEqDeck2: rightDeck.setEqFromController(1, value); break;
            case Action::highEqDeck2: rightDeck.setEqFromController(2, value); break;
            case Action::filterDeck1: leftDeck.setFilterFromController(value); break;
            case Action::filterDeck2: rightDeck.setFilterFromController(value); break;
            case Action::echoDeck1: leftDeck.setEchoFromController(value); break;
            case Action::echoDeck2: rightDeck.setEchoFromController(value); break;
            case Action::masterGain:
                masterGain.setValue(value, juce::sendNotificationSync);
                break;
            case Action::jogDeck1: leftDeck.triggerJog(value); break;
            case Action::jogDeck2: rightDeck.triggerJog(value); break;
            case Action::count: break;
        }
    });

    setSize(1280, 760);
    startTimerHz(24);
}

MainComponent::~MainComponent()
{
    stopTimer();
    midiController.setActionHandler({});
    libraryWindow.reset();
    if (libraryPanel != nullptr)
        removeChildComponent(libraryPanel.get());
    libraryPanel.reset();
    setLookAndFeel(nullptr);
}

void MainComponent::paint(juce::Graphics& graphics)
{
    graphics.fillAll(backgroundColour);
    graphics.setColour(juce::Colour(0x50303840));
    graphics.drawHorizontalLine(51, 12.0f,
                                static_cast<float>(getWidth() - 12));
    auto area = getLocalBounds().reduced(12);
    area.removeFromTop(34);
    area.removeFromTop(6);
    auto mixer = area.removeFromBottom(52);
    area.removeFromBottom(6);
    const auto browserHeight = std::clamp(
        static_cast<int>(area.getHeight() * 0.34f), 132, 260);
    auto browser = area.removeFromBottom(browserHeight);
    graphics.setColour(juce::Colour(0xff141719));
    graphics.fillRect(browser);
    graphics.setColour(juce::Colour(0xff303538));
    graphics.drawRect(browser.toFloat().reduced(0.5f), 0.6f);
    area.removeFromBottom(6);

    const auto overviewHeight = std::clamp(area.getHeight() / 10, 44, 58);
    auto overview = area.removeFromTop(overviewHeight);
    graphics.setColour(juce::Colour(0xff111416));
    graphics.fillRect(overview);
    auto lanes = overview.reduced(8, 5);
    const auto laneGap = 2;
    const auto labelWidth = 42;
    const auto laneHeight = (lanes.getHeight() - laneGap) / 2;
    auto firstLane = lanes.removeFromTop(laneHeight);
    lanes.removeFromTop(laneGap);
    auto secondLane = lanes.removeFromTop(laneHeight);

    graphics.setColour(juce::Colour(0xff29a7dc));
    graphics.setFont(juce::Font(9.0f, juce::Font::bold));
    graphics.drawText("DECK 1", firstLane.removeFromLeft(labelWidth),
                      juce::Justification::centred, false);
    leftDeck.paintMiniOverview(graphics, firstLane.toFloat());

    graphics.setColour(juce::Colour(0xff39c78c));
    graphics.drawText("DECK 2", secondLane.removeFromLeft(labelWidth),
                      juce::Justification::centred, false);
    rightDeck.paintMiniOverview(graphics, secondLane.toFloat());
    graphics.setColour(juce::Colour(0x5030383b));
    graphics.drawHorizontalLine(overview.getCentreY(),
                                static_cast<float>(overview.getX() + 6),
                                static_cast<float>(overview.getRight() - 6));

    graphics.setColour(juce::Colour(0xff141719));
    graphics.fillRect(mixer);
    graphics.setColour(juce::Colour(0x74303840));
    graphics.drawHorizontalLine(mixer.getY(), static_cast<float>(mixer.getX()),
                                static_cast<float>(mixer.getRight()));
    const auto masterDividerX = mixer.getRight() - 12 - 12 - 8 - 48 - 90 - 10;
    graphics.drawVerticalLine(masterDividerX,
                              static_cast<float>(mixer.getY() + 8),
                              static_cast<float>(mixer.getBottom() - 8));
}

void MainComponent::mouseDown(const juce::MouseEvent& event)
{
    overviewDeckBeingDragged = -1;
    const auto lanes = getOverviewWaveformBounds(getWidth(), getHeight());
    const std::array<DeckComponent*, 2> decks { &leftDeck, &rightDeck };
    for (std::size_t index = 0; index < lanes.size(); ++index)
    {
        if (!lanes[index].contains(event.getPosition()))
            continue;

        auto* deck = decks[index];
        if (!deck->hasLoadedTrack() || event.getNumberOfClicks() > 1)
        {
            deck->triggerLoad();
            return;
        }

        overviewDeckBeingDragged = static_cast<int>(index);
        seekOverviewTo(overviewDeckBeingDragged, event.x);
        return;
    }
}

void MainComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (overviewDeckBeingDragged >= 0)
        seekOverviewTo(overviewDeckBeingDragged, event.x);
}

void MainComponent::mouseUp(const juce::MouseEvent&)
{
    overviewDeckBeingDragged = -1;
}

void MainComponent::mouseMove(const juce::MouseEvent& event)
{
    const auto lanes = getOverviewWaveformBounds(getWidth(), getHeight());
    const auto overOverview = std::any_of(
        lanes.begin(), lanes.end(),
        [&event](const auto& bounds) { return bounds.contains(event.getPosition()); });
    setMouseCursor(overOverview ? juce::MouseCursor::PointingHandCursor
                                : juce::MouseCursor::NormalCursor);
}

void MainComponent::mouseExit(const juce::MouseEvent&)
{
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

void MainComponent::seekOverviewTo(int deckIndex, int x)
{
    const auto lanes = getOverviewWaveformBounds(getWidth(), getHeight());
    if (deckIndex < 0 || deckIndex >= static_cast<int>(lanes.size()))
        return;

    const auto& bounds = lanes[static_cast<std::size_t>(deckIndex)];
    const auto start = bounds.getX() + 2;
    const auto width = std::max(1, bounds.getWidth() - 4);
    const auto relativeX = std::clamp(x - start, 0, width);
    const auto normalizedPosition = static_cast<float>(relativeX)
        / static_cast<float>(width);
    (deckIndex == 0 ? leftDeck : rightDeck).seekToNormalized(normalizedPosition);
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(12);
    auto header = area.removeFromTop(34);
    wordmark.setBounds(header.removeFromLeft(94));
    statusLabel.setBounds(header.removeFromLeft(230));
    audioSettingsButton.setBounds(header.removeFromRight(112));
    header.removeFromRight(6);
    libraryButton.setBounds(header.removeFromRight(90));
    header.removeFromRight(6);
    pauseRecordingButton.setBounds(header.removeFromRight(70));
    header.removeFromRight(4);
    recordButton.setBounds(header.removeFromRight(82));
    area.removeFromTop(6);

    auto mixer = area.removeFromBottom(52);
    area.removeFromBottom(6);
    auto browser = area.removeFromBottom(
        std::clamp(static_cast<int>(area.getHeight() * 0.34f), 132, 260));
    libraryPanel->setBounds(browser);
    area.removeFromBottom(6);

    area.removeFromTop(std::clamp(area.getHeight() / 10, 44, 58));
    area.removeFromTop(6);
    const auto deckWidth = (area.getWidth() - 8) / 2;
    auto leftBounds = area.removeFromLeft(deckWidth);
    area.removeFromLeft(8);
    leftDeck.setBounds(leftBounds);
    rightDeck.setBounds(area);

    auto controls = mixer.reduced(12, 6);
    auto faderRow = controls;
    masterMeter.setBounds(faderRow.removeFromRight(12));
    faderRow.removeFromRight(8);
    masterLabel.setBounds(faderRow.removeFromRight(48));
    masterGain.setBounds(faderRow.removeFromRight(90));
    faderRow.removeFromRight(10);
    const auto crossfaderGroupWidth = std::min(480, faderRow.getWidth());
    faderRow.removeFromLeft((faderRow.getWidth() - crossfaderGroupWidth) / 2);
    crossfaderLabel.setBounds(faderRow.removeFromLeft(96));
    crossfader.setBounds(faderRow.removeFromLeft(
        std::max(0, crossfaderGroupWidth - 96)));
}

void MainComponent::timerCallback()
{
    for (const auto& bounds : getOverviewWaveformBounds(getWidth(), getHeight()))
        repaint(bounds);

    masterMeter.setLevel(engine.getMasterLevel());
    if (engine.isRecording())
    {
        statusLabel.setText(engine.getRecordingStatus(), juce::dontSendNotification);
        if (engine.isRecordingPaused() && !pauseRecordingButton.getToggleState())
        {
            pauseRecordingButton.setToggleState(true, juce::dontSendNotification);
            pauseRecordingButton.setButtonText("RESUME");
        }
    }
    else if (recordButton.getButtonText() == "STOP REC")
    {
        recordButton.setButtonText("REC WAV");
        pauseRecordingButton.setEnabled(false);
        pauseRecordingButton.setToggleState(false, juce::dontSendNotification);
        pauseRecordingButton.setButtonText("PAUSE");
        statusLabel.setText(engine.getRecordingStatus(), juce::dontSendNotification);
    }

    const auto leftPlaying = engine.isPlaying(0);
    const auto rightPlaying = engine.isPlaying(1);
    const std::array<bool, 2> deckPlaying { leftPlaying, rightPlaying };
    const std::array<DeckComponent*, 2> deckComponents { &leftDeck, &rightDeck };
    for (std::size_t index = 0; index < deckComponents.size(); ++index)
    {
        const auto revision = deckComponents[index]->getLoadRevision();
        if (deckPlaying[index] && revision > recordedLoadRevisions[index])
        {
            const auto path = deckComponents[index]->getTrackPath();
            const auto title = deckComponents[index]->getTrackTitle();
            if (!library.recordPlay(path, title, static_cast<int>(index)))
                statusLabel.setText("History error: " + library.getLastError(),
                                    juce::dontSendNotification);
            recordedLoadRevisions[index] = revision;
        }
    }

    if (leftPlaying || rightPlaying)
    {
        if (leftPlaying && rightPlaying)
            lastPresenceDeck = engine.getCrossfadePosition() <= 0.5f ? 0U : 1U;
        else
            lastPresenceDeck = leftPlaying ? 0U : 1U;
        hasPresenceTrack = true;
    }

    if (hasPresenceTrack)
    {
        const auto playing = engine.isPlaying(lastPresenceDeck);
        const auto title = lastPresenceDeck == 0
            ? leftDeck.getTrackTitle() : rightDeck.getTrackTitle();
        discordPresence.updateTrack(title, engine.getPosition(lastPresenceDeck),
                                    engine.getLength(lastPresenceDeck), playing);
    }
    else
    {
        discordPresence.updateTrack({}, 0.0, 0.0, false);
    }
}

void MainComponent::openAudioSettings()
{
    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Vyra settings";
    options.dialogBackgroundColour = backgroundColour;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;
    options.content.setOwned(
        new SettingsComponent(engine.getDeviceManager(), discordPresence, midiController, engine));
    options.content->setSize(620, 820);
    options.launchAsync();
}

void MainComponent::openLibrary()
{
    if (libraryWindow == nullptr)
    {
        libraryWindow = std::make_unique<LibraryWindow>(
            library,
            [this](std::size_t deckIndex, library::Track track)
            {
                if (deckIndex == 0)
                {
                    auto title = track.title;
                    if (track.artist.isNotEmpty())
                        title = track.artist + " — " + title;
                    leftDeck.loadFile(juce::File(track.path), std::move(track.waveform),
                                      std::move(title));
                }
                else
                {
                    auto title = track.title;
                    if (track.artist.isNotEmpty())
                        title = track.artist + " — " + title;
                    rightDeck.loadFile(juce::File(track.path), std::move(track.waveform),
                                       std::move(title));
                }
            });
    }

    libraryWindow->setVisible(true);
    libraryWindow->toFront(true);
}
}

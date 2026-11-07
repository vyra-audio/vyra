#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include <atomic>

namespace vyra::audio
{
class LoopRegionAudioSource final : public juce::PositionableAudioSource
{
public:
    explicit LoopRegionAudioSource(juce::AudioFormatReaderSource& source);

    void setLoopRegion(double startSeconds, double endSeconds) noexcept;
    void clearLoop() noexcept;
    [[nodiscard]] bool isRegionLooping() const noexcept;

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override;
    void setNextReadPosition(juce::int64 newPosition) override;
    [[nodiscard]] juce::int64 getNextReadPosition() const override;
    [[nodiscard]] juce::int64 getTotalLength() const override;
    [[nodiscard]] bool isLooping() const override;
    void setLooping(bool shouldLoop) override;

private:
    juce::AudioFormatReaderSource& source;
    std::atomic<juce::int64> loopStart { 0 };
    std::atomic<juce::int64> loopEnd { 0 };
    std::atomic<bool> regionLooping { false };
    std::atomic<bool> wholeTrackLooping { false };
};
}

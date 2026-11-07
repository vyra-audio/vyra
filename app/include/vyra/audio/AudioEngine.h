#pragma once

#include "vyra/audio/ChannelDsp.h"
#include "vyra/audio/LoopRegionAudioSource.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <cstdint>
#include <vector>

namespace vyra::audio
{
class AudioEngine final : private juce::AudioSource
{
public:
    static constexpr std::size_t deckCount = 2;
    using LoadCallback =
        std::function<void(juce::String, std::vector<std::uint8_t>, double, juce::String)>;

    AudioEngine();
    ~AudioEngine() override;

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    void loadTrackAsync(std::size_t deckIndex, juce::File file, LoadCallback callback,
                        std::vector<std::uint8_t> cachedWaveform = {});
    void togglePlay(std::size_t deckIndex);
    void stop(std::size_t deckIndex);
    void cue(std::size_t deckIndex);
    void setPosition(std::size_t deckIndex, double seconds);
    void jumpBeats(std::size_t deckIndex, int beats);
    [[nodiscard]] bool setLoop(std::size_t deckIndex, int beats);
    void clearLoop(std::size_t deckIndex);
    void setDeckPitch(std::size_t deckIndex, float percent) noexcept;
    [[nodiscard]] float getDeckPitch(std::size_t deckIndex) const noexcept;
    [[nodiscard]] double getDeckBpm(std::size_t deckIndex) const noexcept;
    [[nodiscard]] bool syncDeckToDeck(std::size_t targetDeck,
                                      std::size_t referenceDeck) noexcept;
    [[nodiscard]] juce::String startRecording(const juce::File& destination);
    void stopRecording();
    void setRecordingPaused(bool shouldPause) noexcept;
    [[nodiscard]] bool isRecording() const noexcept;
    [[nodiscard]] bool isRecordingPaused() const noexcept;
    [[nodiscard]] juce::String getRecordingStatus() const;
    void setDeckGain(std::size_t deckIndex, float gain) noexcept;
    void setDeckEq(std::size_t deckIndex, float lowDb, float midDb, float highDb) noexcept;
    void setDeckFilter(std::size_t deckIndex, float amount) noexcept;
    void setDeckEcho(std::size_t deckIndex, float wet) noexcept;
    void setCrossfade(float position) noexcept;
    void setMasterGain(float gain) noexcept;
    void setHeadphoneCue(std::size_t deckIndex, bool enabled) noexcept;
    [[nodiscard]] bool isHeadphoneCueEnabled(std::size_t deckIndex) const noexcept;
    void setHeadphoneMonitoringEnabled(bool enabled) noexcept;
    void setHeadphoneMix(float mix) noexcept;
    void setHeadphoneOutputStartChannel(int channelIndex) noexcept;
    [[nodiscard]] int getHeadphoneOutputStartChannel() const noexcept;
    [[nodiscard]] bool isHeadphoneMonitoringEnabled() const noexcept;
    [[nodiscard]] float getCrossfadePosition() const noexcept;

    [[nodiscard]] juce::String getSupportedFileWildcard() const;
    [[nodiscard]] bool isPlaying(std::size_t deckIndex) const;
    [[nodiscard]] double getPosition(std::size_t deckIndex) const;
    [[nodiscard]] double getLength(std::size_t deckIndex) const;
    [[nodiscard]] float getDeckLevel(std::size_t deckIndex) const noexcept;
    [[nodiscard]] float getMasterLevel() const noexcept;
    [[nodiscard]] juce::AudioDeviceManager& getDeviceManager() noexcept;

private:
    struct Deck
    {
        juce::AudioTransportSource transport;
        std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
        std::unique_ptr<LoopRegionAudioSource> loopSource;
        std::unique_ptr<juce::ResamplingAudioSource> resampler;
        std::atomic<float> pitchPercent { 0.0f };
        std::atomic<bool> headphoneCue { false };
        std::atomic<float> channelGain { 1.0f };
        std::atomic<float> lowEqDb { 0.0f };
        std::atomic<float> midEqDb { 0.0f };
        std::atomic<float> highEqDb { 0.0f };
        std::atomic<float> filterAmount { 0.0f };
        std::atomic<float> echoWet { 0.0f };
        std::atomic<float> level { 0.0f };
        std::atomic<double> cuePosition { 0.0 };
        std::atomic<double> bpm { 0.0 };
    };

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;
    [[nodiscard]] Deck* getDeck(std::size_t deckIndex) noexcept;
    [[nodiscard]] const Deck* getDeck(std::size_t deckIndex) const noexcept;

    juce::AudioDeviceManager deviceManager;
    juce::AudioSourcePlayer sourcePlayer;
    juce::AudioFormatManager formatManager;
    juce::TimeSliceThread readAheadThread { "Vyra audio read-ahead" };
    juce::TimeSliceThread recordingThread { "Vyra mix recording" };
    juce::ThreadPool loaderThreads { 1 };
    std::array<Deck, deckCount> decks;
    std::array<juce::AudioBuffer<float>, deckCount> scratchBuffers;
    juce::AudioBuffer<float> headphoneCueBuffer;
    std::array<ThreeBandEqualizer, deckCount> equalizers;
    std::array<std::array<ChannelFilter, 2>, deckCount> filters;
    std::array<EchoEffect, deckCount> echoes;
    std::atomic<float> crossfadePosition { 0.5f };
    std::atomic<float> masterGain { 1.0f };
    std::atomic<float> masterLevel { 0.0f };
    std::atomic<float> headphoneMix { 0.5f };
    std::atomic<int> headphoneOutputStartChannel { 2 };
    std::atomic<bool> headphoneMonitoringEnabled { false };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> recordingWriter;
    std::atomic<juce::AudioFormatWriter::ThreadedWriter*> activeRecordingWriter { nullptr };
    std::atomic<int> recordingWriterUsers { 0 };
    std::atomic<bool> recordingPaused { false };
    std::atomic<bool> recordingOverflow { false };
    std::atomic<juce::int64> recordingSamples { 0 };
    mutable juce::CriticalSection recordingStateLock;
    juce::String recordingStatus { "Recording stopped" };
    std::atomic<double> currentSampleRate { 44100.0 };
    float limiterGain = 1.0f;
};
}

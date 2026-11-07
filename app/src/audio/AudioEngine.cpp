#include "vyra/audio/AudioEngine.h"

#include "vyra/audio/BpmAnalysis.h"
#include "vyra/audio/KeyAnalysis.h"
#include "vyra/audio/MixMath.h"
#include "vyra/audio/Waveform.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <utility>

namespace vyra::audio
{
namespace
{
constexpr float limiterCeiling = 0.98f;
constexpr double limiterReleaseSeconds = 0.08;
constexpr int readAheadBufferSize = 32768;
}

AudioEngine::AudioEngine()
{
    formatManager.registerBasicFormats();
    readAheadThread.startThread();
    if (!recordingThread.startThread())
    {
        readAheadThread.stopThread(2000);
        throw std::runtime_error("Could not start the mix recording worker thread.");
    }

    const auto error = deviceManager.initialise(0, 2, nullptr, true);
    if (error.isNotEmpty())
    {
        recordingThread.stopThread(2000);
        readAheadThread.stopThread(2000);
        throw std::runtime_error(error.toStdString());
    }

    sourcePlayer.setSource(this);
    for (auto& deck : decks)
    {
        deck.resampler = std::make_unique<juce::ResamplingAudioSource>(
            &deck.transport, false, 2);
    }
    deviceManager.addAudioCallback(&sourcePlayer);
}

AudioEngine::~AudioEngine()
{
    deviceManager.removeAudioCallback(&sourcePlayer);
    sourcePlayer.setSource(nullptr);
    stopRecording();
    loaderThreads.removeAllJobs(true, -1);

    for (auto& deck : decks)
    {
        deck.transport.stop();
        deck.transport.setSource(nullptr);
        deck.loopSource.reset();
        deck.readerSource.reset();
        deck.resampler.reset();
    }

    recordingThread.stopThread(2000);
    readAheadThread.stopThread(2000);
}

void AudioEngine::loadTrackAsync(std::size_t deckIndex, juce::File file,
                                 LoadCallback callback,
                                 std::vector<std::uint8_t> cachedWaveform)
{
    if (getDeck(deckIndex) == nullptr)
    {
        if (callback)
            callback("Invalid deck selection.", {}, 0.0, {});
        return;
    }

    loaderThreads.addJob([this, deckIndex, file = std::move(file),
                          callback = std::move(callback),
                          waveform = std::move(cachedWaveform)]() mutable
    {
        juce::String error;
        juce::String musicalKey;
        double bpm = 0.0;
        std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));

        if (reader == nullptr)
        {
            error = "Vyra could not decode this file. Check that the format is supported and the file is readable.";
        }
        else
        {
            bpm = analyzeBpm(*reader);
            musicalKey = analyzeMusicalKey(*reader);
            decks[deckIndex].bpm.store(bpm, std::memory_order_relaxed);
            auto readerSource = std::make_unique<juce::AudioFormatReaderSource>(reader.release(), true);
            const auto sourceSampleRate = readerSource->getAudioFormatReader()->sampleRate;
            auto& transport = decks[deckIndex].transport;
            transport.stop();
            transport.setSource(nullptr);
            decks[deckIndex].loopSource.reset();
            decks[deckIndex].readerSource.reset();
            decks[deckIndex].readerSource = std::move(readerSource);
            decks[deckIndex].loopSource = std::make_unique<LoopRegionAudioSource>(
                *decks[deckIndex].readerSource);
            transport.setSource(decks[deckIndex].loopSource.get(), readAheadBufferSize,
                                &readAheadThread, sourceSampleRate, 2);
            decks[deckIndex].loopSource->clearLoop();
            transport.setPosition(0.0);
            decks[deckIndex].cuePosition.store(0.0, std::memory_order_relaxed);
            decks[deckIndex].pitchPercent.store(0.0f, std::memory_order_relaxed);
            decks[deckIndex].resampler->setResamplingRatio(1.0);

            if (waveform.empty())
            {
                std::unique_ptr<juce::AudioFormatReader> waveformReader(
                    formatManager.createReaderFor(file));
                if (waveformReader != nullptr)
                    waveform = analyzeWaveform(*waveformReader);
            }
        }

        if (callback)
        {
            juce::MessageManager::callAsync(
                [callback = std::move(callback), error = std::move(error),
                 waveform = std::move(waveform), bpm,
                 musicalKey = std::move(musicalKey)]() mutable
                {
                    callback(std::move(error), std::move(waveform), bpm,
                             std::move(musicalKey));
                });
        }
    });
}

void AudioEngine::togglePlay(std::size_t deckIndex)
{
    if (auto* deck = getDeck(deckIndex))
    {
        if (deck->transport.isPlaying())
            deck->transport.stop();
        else if (deck->transport.getLengthInSeconds() > 0.0)
        {
            if (deck->transport.getCurrentPosition() >= deck->transport.getLengthInSeconds())
                deck->transport.setPosition(0.0);
            deck->transport.start();
        }
    }
}

void AudioEngine::stop(std::size_t deckIndex)
{
    if (auto* deck = getDeck(deckIndex))
    {
        deck->transport.stop();
        deck->transport.setPosition(0.0);
    }
}

void AudioEngine::cue(std::size_t deckIndex)
{
    if (auto* deck = getDeck(deckIndex))
    {
        if (deck->transport.isPlaying())
        {
            deck->cuePosition.store(deck->transport.getCurrentPosition(), std::memory_order_relaxed);
        }
        else
        {
            deck->transport.setPosition(deck->cuePosition.load(std::memory_order_relaxed));
        }
    }
}

void AudioEngine::setPosition(std::size_t deckIndex, double seconds)
{
    if (auto* deck = getDeck(deckIndex))
        deck->transport.setPosition(std::max(0.0, seconds));
}

void AudioEngine::jumpBeats(std::size_t deckIndex, int beats)
{
    if (auto* deck = getDeck(deckIndex))
    {
        const auto nextPosition = beatJumpPosition(
            deck->transport.getCurrentPosition(),
            deck->transport.getLengthInSeconds(),
            deck->bpm.load(std::memory_order_relaxed)
                * pitchRatioFromPercent(
                    deck->pitchPercent.load(std::memory_order_relaxed)),
            beats);
        deck->transport.setPosition(nextPosition);
    }
}

bool AudioEngine::setLoop(std::size_t deckIndex, int beats)
{
    auto* deck = getDeck(deckIndex);
    if (deck == nullptr || deck->loopSource == nullptr || beats <= 0)
        return false;

    const auto bpm = deck->bpm.load(std::memory_order_relaxed)
        * pitchRatioFromPercent(deck->pitchPercent.load(std::memory_order_relaxed));
    const auto length = deck->transport.getLengthInSeconds();
    if (bpm <= 0.0 || length <= 0.0)
        return false;

    const auto start = deck->transport.getCurrentPosition();
    const auto end = std::min(length, start + static_cast<double>(beats) * 60.0 / bpm);
    if (end > start)
        deck->loopSource->setLoopRegion(start, end);
    return deck->loopSource->isRegionLooping();
}

void AudioEngine::clearLoop(std::size_t deckIndex)
{
    if (auto* deck = getDeck(deckIndex); deck != nullptr && deck->loopSource != nullptr)
        deck->loopSource->clearLoop();
}

void AudioEngine::setDeckPitch(std::size_t deckIndex, float percent) noexcept
{
    if (auto* deck = getDeck(deckIndex))
    {
        const auto bounded = std::clamp(percent, -16.0f, 16.0f);
        deck->pitchPercent.store(bounded, std::memory_order_relaxed);
        if (deck->resampler != nullptr)
            deck->resampler->setResamplingRatio(pitchRatioFromPercent(bounded));
    }
}

float AudioEngine::getDeckPitch(std::size_t deckIndex) const noexcept
{
    if (const auto* deck = getDeck(deckIndex))
        return deck->pitchPercent.load(std::memory_order_relaxed);
    return 0.0f;
}

double AudioEngine::getDeckBpm(std::size_t deckIndex) const noexcept
{
    if (const auto* deck = getDeck(deckIndex))
        return deck->bpm.load(std::memory_order_relaxed);
    return 0.0;
}

bool AudioEngine::syncDeckToDeck(std::size_t targetDeck,
                                 std::size_t referenceDeck) noexcept
{
    if (targetDeck == referenceDeck)
        return false;
    auto* target = getDeck(targetDeck);
    auto* reference = getDeck(referenceDeck);
    if (target == nullptr || reference == nullptr)
        return false;

    const auto targetBpm = target->bpm.load(std::memory_order_relaxed);
    const auto referenceBpm = reference->bpm.load(std::memory_order_relaxed);
    if (targetBpm <= 0.0 || referenceBpm <= 0.0)
        return false;

    setDeckPitch(targetDeck, tempoSyncPitchPercent(targetBpm, referenceBpm));
    return true;
}

juce::String AudioEngine::startRecording(const juce::File& destination)
{
    const juce::ScopedLock lock(recordingStateLock);
    if (activeRecordingWriter.load(std::memory_order_acquire) != nullptr)
        return "A recording is already active.";
    if (destination == juce::File {} || destination.getFileExtension().compareIgnoreCase(".wav") != 0)
        return "Choose a destination with the .wav extension.";

    auto stream = destination.createOutputStream();
    if (stream == nullptr)
        return "Could not create the recording file. Check the destination and permissions.";

    juce::WavAudioFormat wavFormat;
    const auto sampleRate = currentSampleRate.load(std::memory_order_relaxed);
    auto* writer = wavFormat.createWriterFor(stream.get(),
        sampleRate > 0.0 ? sampleRate : 44100.0,
        2, 24, {}, 0);
    if (writer == nullptr)
        return "Could not initialize the WAV recording encoder.";

    stream.release();
    recordingWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(
        writer, recordingThread, 32768);
    recordingPaused.store(false, std::memory_order_relaxed);
    recordingOverflow.store(false, std::memory_order_relaxed);
    recordingSamples.store(0, std::memory_order_relaxed);
    recordingStatus = "Recording WAV: " + destination.getFileName();
    activeRecordingWriter.store(recordingWriter.get(), std::memory_order_release);
    return {};
}

void AudioEngine::stopRecording()
{
    juce::AudioFormatWriter::ThreadedWriter* writer = nullptr;
    {
        const juce::ScopedLock lock(recordingStateLock);
        writer = activeRecordingWriter.exchange(nullptr, std::memory_order_acq_rel);
        if (writer == nullptr)
            return;
    }

    while (recordingWriterUsers.load(std::memory_order_acquire) != 0)
        juce::Thread::sleep(1);

    {
        const juce::ScopedLock lock(recordingStateLock);
        recordingWriter.reset();
        recordingPaused.store(false, std::memory_order_relaxed);
        const auto overflowed = recordingOverflow.load(std::memory_order_relaxed);
        const auto sampleRate = currentSampleRate.load(std::memory_order_relaxed);
        const auto seconds = static_cast<double>(
            recordingSamples.load(std::memory_order_relaxed))
            / (sampleRate > 0.0 ? sampleRate : 44100.0);
        recordingStatus = overflowed
            ? "Recording stopped; audio buffer overflow caused dropped samples."
            : "Recording saved (" + juce::String(seconds, 1) + " s)";
    }
}

void AudioEngine::setRecordingPaused(bool shouldPause) noexcept
{
    if (activeRecordingWriter.load(std::memory_order_acquire) != nullptr)
        recordingPaused.store(shouldPause, std::memory_order_release);
}

bool AudioEngine::isRecording() const noexcept
{
    return activeRecordingWriter.load(std::memory_order_acquire) != nullptr;
}

bool AudioEngine::isRecordingPaused() const noexcept
{
    return recordingPaused.load(std::memory_order_relaxed);
}

juce::String AudioEngine::getRecordingStatus() const
{
    const juce::ScopedLock lock(recordingStateLock);
    return recordingStatus;
}

void AudioEngine::setDeckGain(std::size_t deckIndex, float gain) noexcept
{
    if (auto* deck = getDeck(deckIndex))
        deck->channelGain.store(std::clamp(gain, 0.0f, 1.0f), std::memory_order_relaxed);
}

void AudioEngine::setDeckEq(std::size_t deckIndex, float lowDb,
                            float midDb, float highDb) noexcept
{
    if (auto* deck = getDeck(deckIndex))
    {
        deck->lowEqDb.store(std::clamp(lowDb, -12.0f, 12.0f), std::memory_order_relaxed);
        deck->midEqDb.store(std::clamp(midDb, -12.0f, 12.0f), std::memory_order_relaxed);
        deck->highEqDb.store(std::clamp(highDb, -12.0f, 12.0f), std::memory_order_relaxed);
    }
}

void AudioEngine::setDeckFilter(std::size_t deckIndex, float amount) noexcept
{
    if (auto* deck = getDeck(deckIndex))
        deck->filterAmount.store(std::clamp(amount, -1.0f, 1.0f), std::memory_order_relaxed);
}

void AudioEngine::setDeckEcho(std::size_t deckIndex, float wet) noexcept
{
    if (auto* deck = getDeck(deckIndex))
        deck->echoWet.store(std::clamp(wet, 0.0f, 0.8f), std::memory_order_relaxed);
}

void AudioEngine::setCrossfade(float position) noexcept
{
    crossfadePosition.store(std::clamp(position, 0.0f, 1.0f), std::memory_order_relaxed);
}

void AudioEngine::setMasterGain(float gain) noexcept
{
    masterGain.store(std::clamp(gain, 0.0f, 1.0f), std::memory_order_relaxed);
}

void AudioEngine::setHeadphoneCue(std::size_t deckIndex, bool enabled) noexcept
{
    if (auto* deck = getDeck(deckIndex))
        deck->headphoneCue.store(enabled, std::memory_order_relaxed);
}

bool AudioEngine::isHeadphoneCueEnabled(std::size_t deckIndex) const noexcept
{
    if (const auto* deck = getDeck(deckIndex))
        return deck->headphoneCue.load(std::memory_order_relaxed);
    return false;
}

void AudioEngine::setHeadphoneMonitoringEnabled(bool enabled) noexcept
{
    headphoneMonitoringEnabled.store(enabled, std::memory_order_release);
    if (!enabled)
    {
        for (auto& deck : decks)
            deck.headphoneCue.store(false, std::memory_order_relaxed);
    }
}

void AudioEngine::setHeadphoneMix(float mix) noexcept
{
    headphoneMix.store(std::clamp(mix, 0.0f, 1.0f), std::memory_order_relaxed);
}

void AudioEngine::setHeadphoneOutputStartChannel(int channelIndex) noexcept
{
    headphoneOutputStartChannel.store(std::max(2, channelIndex),
                                      std::memory_order_relaxed);
}

int AudioEngine::getHeadphoneOutputStartChannel() const noexcept
{
    return headphoneOutputStartChannel.load(std::memory_order_relaxed);
}

bool AudioEngine::isHeadphoneMonitoringEnabled() const noexcept
{
    return headphoneMonitoringEnabled.load(std::memory_order_acquire);
}

float AudioEngine::getCrossfadePosition() const noexcept
{
    return crossfadePosition.load(std::memory_order_relaxed);
}

juce::String AudioEngine::getSupportedFileWildcard() const
{
    return formatManager.getWildcardForAllFormats();
}

bool AudioEngine::isPlaying(std::size_t deckIndex) const
{
    if (const auto* deck = getDeck(deckIndex))
        return deck->transport.isPlaying();
    return false;
}

double AudioEngine::getPosition(std::size_t deckIndex) const
{
    if (const auto* deck = getDeck(deckIndex))
        return deck->transport.getCurrentPosition();
    return 0.0;
}

double AudioEngine::getLength(std::size_t deckIndex) const
{
    if (const auto* deck = getDeck(deckIndex))
        return deck->transport.getLengthInSeconds();
    return 0.0;
}

float AudioEngine::getDeckLevel(std::size_t deckIndex) const noexcept
{
    if (const auto* deck = getDeck(deckIndex))
        return deck->level.load(std::memory_order_relaxed);
    return 0.0f;
}

float AudioEngine::getMasterLevel() const noexcept
{
    return masterLevel.load(std::memory_order_relaxed);
}

juce::AudioDeviceManager& AudioEngine::getDeviceManager() noexcept
{
    return deviceManager;
}

void AudioEngine::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    currentSampleRate.store(sampleRate > 0.0 ? sampleRate : 44100.0,
                             std::memory_order_relaxed);
    const auto activeSampleRate = currentSampleRate.load(std::memory_order_relaxed);
    const auto bufferSize = std::max(1, samplesPerBlockExpected);
    headphoneCueBuffer.setSize(2, bufferSize, false, false, true);

    for (std::size_t index = 0; index < decks.size(); ++index)
    {
        scratchBuffers[index].setSize(2, bufferSize, false, false, true);
        if (decks[index].resampler != nullptr)
            decks[index].resampler->prepareToPlay(bufferSize, activeSampleRate);
        equalizers[index].prepare(activeSampleRate);
        for (auto& filter : filters[index])
            filter.prepare(activeSampleRate);
        echoes[index].prepare(activeSampleRate);
        decks[index].level.store(0.0f, std::memory_order_relaxed);
    }
}

void AudioEngine::getNextAudioBlock(const juce::AudioSourceChannelInfo& output)
{
    if (output.buffer == nullptr || output.numSamples <= 0)
        return;

    output.clearActiveBufferRegion();

    const auto gains = equalPowerCrossfade(crossfadePosition.load(std::memory_order_relaxed));
    const auto maxChunk = scratchBuffers[0].getNumSamples();
    if (maxChunk <= 0)
        return;

    auto outputOffset = output.startSample;
    auto remaining = output.numSamples;
    float blockOutputPeak = 0.0f;
    const auto activeSampleRate = currentSampleRate.load(std::memory_order_relaxed);

    while (remaining > 0)
    {
        const auto chunk = std::min(remaining, maxChunk);
        const auto headphoneEnabled =
            headphoneMonitoringEnabled.load(std::memory_order_acquire);
        if (headphoneEnabled)
            headphoneCueBuffer.clear(0, chunk);

        for (std::size_t index = 0; index < decks.size(); ++index)
        {
            auto& scratch = scratchBuffers[index];
            scratch.clear(0, chunk);
            if (decks[index].resampler != nullptr)
                decks[index].resampler->getNextAudioBlock({ &scratch, 0, chunk });

            const auto crossfadeGain = index == 0 ? gains.first : gains.second;
            const auto channelGain = decks[index].channelGain.load(std::memory_order_relaxed);
            const auto gain = crossfadeGain * channelGain;
            auto& deck = decks[index];
            equalizers[index].configure(deck.lowEqDb.load(std::memory_order_relaxed),
                                        deck.midEqDb.load(std::memory_order_relaxed),
                                        deck.highEqDb.load(std::memory_order_relaxed));
            const auto filterAmount = deck.filterAmount.load(std::memory_order_relaxed);
            const auto echoWet = deck.echoWet.load(std::memory_order_relaxed);
            const auto cueEnabled = headphoneEnabled
                && deck.headphoneCue.load(std::memory_order_relaxed);
            float deckPeak = 0.0f;

            const auto channels = std::min({ 2, scratch.getNumChannels(), output.buffer->getNumChannels() });
            for (int sample = 0; sample < chunk; ++sample)
            {
                for (int channel = 0; channel < channels; ++channel)
                {
                    auto value = scratch.getSample(channel, sample);
                    value = filters[index][static_cast<std::size_t>(channel)].process(value, filterAmount);
                    value = equalizers[index].process(static_cast<std::size_t>(channel), value);
                    value = echoes[index].process(static_cast<std::size_t>(channel), value, echoWet);
                    if (cueEnabled)
                        headphoneCueBuffer.addSample(channel, sample, value);
                    value *= gain;
                    output.buffer->addSample(channel, outputOffset + sample, value);
                    deckPeak = std::max(deckPeak, std::abs(value));
                }
                echoes[index].advance();
            }

            decks[index].level.store(deckPeak * gain, std::memory_order_relaxed);
        }

        float peak = 0.0f;
        const auto outputChannels = std::min(2, output.buffer->getNumChannels());
        for (int channel = 0; channel < outputChannels; ++channel)
        {
            for (int sample = 0; sample < chunk; ++sample)
                peak = std::max(peak,
                                std::abs(output.buffer->getSample(channel, outputOffset + sample)));
        }

        const auto master = masterGain.load(std::memory_order_relaxed);
        const auto targetGain = limiterTargetGain(peak * master, limiterCeiling);
        if (targetGain < limiterGain)
        {
            limiterGain = targetGain;
        }
        else
        {
            const auto release = 1.0f - static_cast<float>(
                std::exp(-static_cast<double>(chunk) / (activeSampleRate * limiterReleaseSeconds)));
            limiterGain += (targetGain - limiterGain) * release;
        }

        const auto appliedGain = limiterGain * master;
        output.buffer->applyGain(outputOffset, chunk, appliedGain);

        if (headphoneEnabled)
        {
            const auto headphoneMixAmount =
                headphoneMix.load(std::memory_order_relaxed);
            const auto headphoneStart =
                headphoneOutputStartChannel.load(std::memory_order_relaxed);
            const auto hasHeadphonePair = headphoneStart >= 2
                && headphoneStart + 1 < output.buffer->getNumChannels();
            if (hasHeadphonePair)
            {
                float headphonePeak = 0.0f;
                for (int sample = 0; sample < chunk; ++sample)
                {
                    for (int channel = 0; channel < 2; ++channel)
                    {
                        const auto cueValue = headphoneCueBuffer.getSample(channel, sample);
                        const auto masterValue = output.buffer->getSample(
                            channel, outputOffset + sample);
                        const auto value = headphoneMonitorSample(
                            cueValue, masterValue, headphoneMixAmount);
                        headphoneCueBuffer.setSample(channel, sample, value);
                        headphonePeak = std::max(headphonePeak, std::abs(value));
                    }
                }

                const auto headphoneGain = limiterTargetGain(headphonePeak, limiterCeiling);
                for (int sample = 0; sample < chunk; ++sample)
                {
                    for (int channel = 0; channel < 2; ++channel)
                    {
                        output.buffer->setSample(
                            headphoneStart + channel, outputOffset + sample,
                            headphoneCueBuffer.getSample(channel, sample) * headphoneGain);
                    }
                }
            }
        }

        auto* writer = activeRecordingWriter.load(std::memory_order_acquire);
        if (writer != nullptr && !recordingPaused.load(std::memory_order_relaxed)
            && output.buffer->getNumChannels() > 0)
        {
            recordingWriterUsers.fetch_add(1, std::memory_order_acq_rel);
            if (writer == activeRecordingWriter.load(std::memory_order_acquire))
            {
                const auto leftChannel = output.buffer->getReadPointer(
                    0, outputOffset);
                const auto rightChannel = output.buffer->getReadPointer(
                    std::min(1, output.buffer->getNumChannels() - 1), outputOffset);
                const std::array<const float*, 2> channels { leftChannel, rightChannel };
                if (writer->write(channels.data(), chunk))
                    recordingSamples.fetch_add(chunk, std::memory_order_relaxed);
                else
                    recordingOverflow.store(true, std::memory_order_relaxed);
            }
            recordingWriterUsers.fetch_sub(1, std::memory_order_release);
        }

        float outputPeak = 0.0f;
        for (int channel = 0; channel < std::min(2, output.buffer->getNumChannels()); ++channel)
        {
            for (int sample = 0; sample < chunk; ++sample)
                outputPeak = std::max(outputPeak,
                                      std::abs(output.buffer->getSample(channel, outputOffset + sample)));
        }
        blockOutputPeak = std::max(blockOutputPeak, outputPeak);

        outputOffset += chunk;
        remaining -= chunk;
    }

    masterLevel.store(blockOutputPeak, std::memory_order_relaxed);
}

void AudioEngine::releaseResources()
{
    for (auto& deck : decks)
    {
        if (deck.resampler != nullptr)
            deck.resampler->releaseResources();
    }
}

AudioEngine::Deck* AudioEngine::getDeck(std::size_t deckIndex) noexcept
{
    return deckIndex < decks.size() ? &decks[deckIndex] : nullptr;
}

const AudioEngine::Deck* AudioEngine::getDeck(std::size_t deckIndex) const noexcept
{
    return deckIndex < decks.size() ? &decks[deckIndex] : nullptr;
}
}

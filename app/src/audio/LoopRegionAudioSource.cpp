#include "vyra/audio/LoopRegionAudioSource.h"

#include <algorithm>
#include <cmath>

namespace vyra::audio
{
LoopRegionAudioSource::LoopRegionAudioSource(juce::AudioFormatReaderSource& readerSource)
    : source(readerSource)
{
}

void LoopRegionAudioSource::setLoopRegion(double startSeconds,
                                          double endSeconds) noexcept
{
    const auto* reader = source.getAudioFormatReader();
    if (reader == nullptr || reader->sampleRate <= 0.0
        || !std::isfinite(startSeconds) || !std::isfinite(endSeconds)
        || startSeconds < 0.0 || endSeconds <= startSeconds)
    {
        clearLoop();
        return;
    }

    const auto start = static_cast<juce::int64>(startSeconds * reader->sampleRate);
    const auto end = std::min(static_cast<juce::int64>(endSeconds * reader->sampleRate),
                              source.getTotalLength());
    if (end <= start)
    {
        clearLoop();
        return;
    }

    loopStart.store(start, std::memory_order_relaxed);
    loopEnd.store(end, std::memory_order_relaxed);
    regionLooping.store(true, std::memory_order_release);
}

void LoopRegionAudioSource::clearLoop() noexcept
{
    regionLooping.store(false, std::memory_order_release);
}

bool LoopRegionAudioSource::isRegionLooping() const noexcept
{
    return regionLooping.load(std::memory_order_acquire);
}

void LoopRegionAudioSource::prepareToPlay(int samplesPerBlockExpected,
                                          double sampleRate)
{
    source.prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void LoopRegionAudioSource::releaseResources()
{
    source.releaseResources();
}

void LoopRegionAudioSource::getNextAudioBlock(
    const juce::AudioSourceChannelInfo& info)
{
    if (!regionLooping.load(std::memory_order_acquire))
    {
        source.getNextAudioBlock(info);
        return;
    }

    const auto start = loopStart.load(std::memory_order_relaxed);
    const auto end = loopEnd.load(std::memory_order_relaxed);
    if (end <= start)
    {
        source.getNextAudioBlock(info);
        return;
    }

    auto outputOffset = info.startSample;
    auto remaining = info.numSamples;
    while (remaining > 0)
    {
        auto position = source.getNextReadPosition();
        if (position < start || position >= end)
        {
            position = start;
            source.setNextReadPosition(position);
        }

        const auto untilBoundary = static_cast<int>(std::min<juce::int64>(
            remaining, end - position));
        if (untilBoundary <= 0)
        {
            info.buffer->clear(outputOffset, remaining);
            return;
        }

        source.getNextAudioBlock({ info.buffer, outputOffset, untilBoundary });
        outputOffset += untilBoundary;
        remaining -= untilBoundary;
    }
}

void LoopRegionAudioSource::setNextReadPosition(juce::int64 newPosition)
{
    source.setNextReadPosition(newPosition);
}

juce::int64 LoopRegionAudioSource::getNextReadPosition() const
{
    return source.getNextReadPosition();
}

juce::int64 LoopRegionAudioSource::getTotalLength() const
{
    return source.getTotalLength();
}

bool LoopRegionAudioSource::isLooping() const
{
    return isRegionLooping() || wholeTrackLooping.load(std::memory_order_relaxed);
}

void LoopRegionAudioSource::setLooping(bool shouldLoop)
{
    wholeTrackLooping.store(shouldLoop, std::memory_order_relaxed);
    source.setLooping(shouldLoop);
}
}

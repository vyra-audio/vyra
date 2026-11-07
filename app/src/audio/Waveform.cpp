#include "vyra/audio/Waveform.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace vyra::audio
{
std::vector<std::uint8_t> analyzeWaveform(juce::AudioFormatReader& reader,
                                          int binCount)
{
    const auto sampleCount = reader.lengthInSamples;
    if (binCount <= 0 || sampleCount <= 0 || reader.numChannels <= 0)
        return {};

    std::vector<float> peaks(static_cast<std::size_t>(binCount), 0.0f);
    juce::AudioBuffer<float> buffer(
        std::min(2, static_cast<int>(reader.numChannels)), 8192);
    auto samplePosition = juce::int64 {};

    while (samplePosition < sampleCount)
    {
        const auto samplesToRead = static_cast<int>(
            std::min<juce::int64>(buffer.getNumSamples(), sampleCount - samplePosition));
        if (!reader.read(&buffer, 0, samplesToRead, samplePosition, true, true))
            return {};

        for (int sample = 0; sample < samplesToRead; ++sample)
        {
            const auto absoluteSample = samplePosition + sample;
            const auto bin = static_cast<std::size_t>(
                static_cast<long double>(absoluteSample) * binCount / sampleCount);
            float peak = 0.0f;
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                peak = std::max(peak, std::abs(buffer.getSample(channel, sample)));
            const auto boundedBin = std::min(bin, peaks.size() - 1);
            peaks[boundedBin] = std::max(peaks[boundedBin], peak);
        }

        samplePosition += samplesToRead;
    }

    std::vector<std::uint8_t> waveform;
    waveform.reserve(peaks.size());
    for (const auto peak : peaks)
    {
        const auto normalized = std::clamp(peak, 0.0f, 1.0f);
        waveform.push_back(static_cast<std::uint8_t>(
            std::lround(normalized * std::numeric_limits<std::uint8_t>::max())));
    }
    return waveform;
}
}

#include "vyra/audio/BpmAnalysis.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace vyra::audio
{
double analyzeBpm(juce::AudioFormatReader& reader)
{
    constexpr double analysisDurationSeconds = 120.0;
    constexpr double analysisRate = 100.0;
    constexpr double minimumBpm = 70.0;
    constexpr double maximumBpm = 180.0;

    if (reader.sampleRate <= 0.0 || reader.lengthInSamples <= 0
        || reader.numChannels <= 0)
        return 0.0;

    const auto frameSamples = std::max<juce::int64>(
        1, static_cast<juce::int64>(std::llround(reader.sampleRate / analysisRate)));
    const auto samplesToAnalyze = std::min<juce::int64>(
        reader.lengthInSamples,
        static_cast<juce::int64>(std::llround(
            reader.sampleRate * analysisDurationSeconds)));
    const auto frameCount = static_cast<std::size_t>(
        (samplesToAnalyze + frameSamples - 1) / frameSamples);
    if (frameCount < 1200)
        return 0.0;

    juce::AudioBuffer<float> buffer(
        std::min(2, static_cast<int>(reader.numChannels)), 8192);
    std::vector<float> energy;
    energy.reserve(frameCount);
    double frameEnergy = 0.0;
    juce::int64 frameSampleCount = 0;
    juce::int64 absolutePosition = 0;

    while (absolutePosition < samplesToAnalyze)
    {
        const auto count = static_cast<int>(std::min<juce::int64>(
            buffer.getNumSamples(), samplesToAnalyze - absolutePosition));
        if (!reader.read(&buffer, 0, count, absolutePosition, true, true))
            return 0.0;

        for (int sample = 0; sample < count; ++sample)
        {
            double mono = 0.0;
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                mono += buffer.getSample(channel, sample);
            mono /= static_cast<double>(buffer.getNumChannels());
            frameEnergy += mono * mono;
            ++frameSampleCount;

            if (frameSampleCount == frameSamples)
            {
                energy.push_back(static_cast<float>(
                    std::sqrt(frameEnergy / static_cast<double>(frameSampleCount))));
                frameEnergy = 0.0;
                frameSampleCount = 0;
            }
        }

        absolutePosition += count;
    }

    if (frameSampleCount > 0)
    {
        energy.push_back(static_cast<float>(
            std::sqrt(frameEnergy / static_cast<double>(frameSampleCount))));
    }
    if (energy.size() < 1200)
        return 0.0;

    std::vector<float> onset(energy.size(), 0.0f);
    float maximumOnset = 0.0f;
    double totalEnergy = 0.0;
    for (std::size_t index = 1; index < energy.size(); ++index)
    {
        onset[index] = std::max(0.0f, energy[index] - energy[index - 1]);
        maximumOnset = std::max(maximumOnset, onset[index]);
        totalEnergy += energy[index];
    }
    if (maximumOnset < 1.0e-5f || totalEnergy / energy.size() < 1.0e-4)
        return 0.0;

    for (auto& value : onset)
        value /= maximumOnset;

    const auto framesPerSecond = reader.sampleRate / static_cast<double>(frameSamples);
    const auto minimumLag = std::max(1, static_cast<int>(
        std::floor(60.0 * framesPerSecond / maximumBpm)));
    const auto maximumLag = static_cast<int>(
        std::ceil(60.0 * framesPerSecond / minimumBpm));
    std::vector<double> scores(static_cast<std::size_t>(maximumLag + 1), 0.0);

    for (int lag = minimumLag; lag <= maximumLag; ++lag)
    {
        double correlation = 0.0;
        double leftEnergy = 0.0;
        double rightEnergy = 0.0;
        for (std::size_t index = static_cast<std::size_t>(lag);
             index < onset.size(); ++index)
        {
            const auto left = static_cast<double>(onset[index]);
            const auto right = static_cast<double>(onset[index - lag]);
            correlation += left * right;
            leftEnergy += left * left;
            rightEnergy += right * right;
        }

        const auto normalization = std::sqrt(leftEnergy * rightEnergy);
        if (normalization > 0.0)
            scores[static_cast<std::size_t>(lag)] = correlation / normalization;
    }

    auto bestLag = minimumLag;
    for (int lag = minimumLag + 1; lag <= maximumLag; ++lag)
    {
        if (scores[static_cast<std::size_t>(lag)]
            > scores[static_cast<std::size_t>(bestLag)])
            bestLag = lag;
    }

    const auto bestScore = scores[static_cast<std::size_t>(bestLag)];
    if (bestScore < 0.08)
        return 0.0;

    auto refinedLag = static_cast<double>(bestLag);
    if (bestLag > minimumLag && bestLag < maximumLag)
    {
        const auto before = scores[static_cast<std::size_t>(bestLag - 1)];
        const auto centre = bestScore;
        const auto after = scores[static_cast<std::size_t>(bestLag + 1)];
        const auto denominator = before - 2.0 * centre + after;
        if (std::abs(denominator) > 1.0e-9)
        {
            const auto adjustment = 0.5 * (before - after) / denominator;
            refinedLag += std::clamp(adjustment, -0.5, 0.5);
        }
    }

    const auto bpm = 60.0 * framesPerSecond / refinedLag;
    return bpm >= minimumBpm && bpm <= maximumBpm ? bpm : 0.0;
}
}

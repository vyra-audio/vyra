#include "vyra/audio/KeyAnalysis.h"

#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <vector>

namespace vyra::audio
{
namespace
{
constexpr int fftOrder = 14;
constexpr int fftSize = 1 << fftOrder;
constexpr double analysisDurationSeconds = 120.0;
constexpr double frameSpacingSeconds = 2.0;
constexpr std::array<double, 12> majorProfile {
    6.35, 2.23, 3.48, 2.33, 4.38, 4.09,
    2.52, 5.19, 2.39, 3.66, 2.29, 2.88
};
constexpr std::array<double, 12> minorProfile {
    6.33, 2.68, 3.52, 5.38, 2.60, 3.53,
    2.54, 4.75, 3.98, 2.69, 3.34, 3.17
};
constexpr std::array<const char*, 12> keyNames {
    "C", "C#", "D", "D#", "E", "F",
    "F#", "G", "G#", "A", "A#", "B"
};

double correlation(const std::array<double, 12>& chroma,
                   const std::array<double, 12>& profile,
                   int root)
{
    double chromaMean = 0.0;
    double profileMean = 0.0;
    for (int index = 0; index < 12; ++index)
    {
        chromaMean += chroma[static_cast<std::size_t>(index)];
        profileMean += profile[static_cast<std::size_t>(index)];
    }
    chromaMean /= 12.0;
    profileMean /= 12.0;

    double numerator = 0.0;
    double chromaNorm = 0.0;
    double profileNorm = 0.0;
    for (int index = 0; index < 12; ++index)
    {
        const auto chromaValue = chroma[static_cast<std::size_t>((root + index) % 12)]
            - chromaMean;
        const auto profileValue = profile[static_cast<std::size_t>(index)] - profileMean;
        numerator += chromaValue * profileValue;
        chromaNorm += chromaValue * chromaValue;
        profileNorm += profileValue * profileValue;
    }
    const auto denominator = std::sqrt(chromaNorm * profileNorm);
    return denominator > 0.0 ? numerator / denominator : 0.0;
}
}

juce::String analyzeMusicalKey(juce::AudioFormatReader& reader)
{
    if (reader.sampleRate <= 0.0 || reader.lengthInSamples <= 0
        || reader.numChannels <= 0)
        return {};

    const auto sampleLimit = std::min<juce::int64>(
        reader.lengthInSamples,
        static_cast<juce::int64>(reader.sampleRate * analysisDurationSeconds));
    if (sampleLimit < fftSize * 2)
        return {};

    juce::dsp::FFT fft(fftOrder);
    juce::dsp::WindowingFunction<float> window(
        static_cast<std::size_t>(fftSize),
        juce::dsp::WindowingFunction<float>::hann, false);
    juce::AudioBuffer<float> audio(std::min(2, static_cast<int>(reader.numChannels)),
                                   fftSize);
    std::vector<float> spectrum(static_cast<std::size_t>(fftSize * 2), 0.0f);
    std::array<double, 12> chroma {};
    int analyzedFrames = 0;
    const auto frameSpacing = static_cast<juce::int64>(
        std::max(1.0, std::round(reader.sampleRate * frameSpacingSeconds)));

    for (juce::int64 offset = 0; offset + fftSize <= sampleLimit;
         offset += frameSpacing)
    {
        audio.clear();
        if (!reader.read(&audio, 0, fftSize, offset, true, true))
            return {};

        for (int sample = 0; sample < fftSize; ++sample)
        {
            double mono = 0.0;
            for (int channel = 0; channel < audio.getNumChannels(); ++channel)
                mono += audio.getSample(channel, sample);
            mono /= static_cast<double>(audio.getNumChannels());
            spectrum[static_cast<std::size_t>(sample)] = static_cast<float>(mono);
        }
        std::fill(spectrum.begin() + fftSize, spectrum.end(), 0.0f);
        window.multiplyWithWindowingTable(spectrum.data(), fftSize);
        fft.performFrequencyOnlyForwardTransform(spectrum.data(), true);

        double frameMagnitude = 0.0;
        std::array<double, 12> frameChroma {};
        for (int bin = 1; bin <= fftSize / 2; ++bin)
        {
            const auto frequency = static_cast<double>(bin) * reader.sampleRate / fftSize;
            if (frequency < 55.0 || frequency > 1400.0)
                continue;

            const auto midiNote = static_cast<int>(std::lround(
                69.0 + 12.0 * std::log2(frequency / 440.0)));
            auto pitchClass = midiNote % 12;
            if (pitchClass < 0)
                pitchClass += 12;
            const auto magnitude = static_cast<double>(spectrum[static_cast<std::size_t>(bin)])
                / std::sqrt(static_cast<double>(bin));
            frameChroma[static_cast<std::size_t>(pitchClass)] += magnitude;
            frameMagnitude += magnitude;
        }
        if (frameMagnitude <= 1.0e-8)
            continue;

        for (std::size_t pitchClass = 0; pitchClass < chroma.size(); ++pitchClass)
            chroma[pitchClass] += frameChroma[pitchClass] / frameMagnitude;
        ++analyzedFrames;
    }

    if (analyzedFrames < 2)
        return {};

    const auto total = std::accumulate(chroma.begin(), chroma.end(), 0.0);
    if (total <= 0.0)
        return {};
    for (auto& value : chroma)
        value /= total;

    auto bestScore = -1.0;
    int bestRoot = 0;
    bool bestIsMinor = false;
    for (int root = 0; root < 12; ++root)
    {
        const auto major = correlation(chroma, majorProfile, root);
        const auto minor = correlation(chroma, minorProfile, root);
        if (major > bestScore)
        {
            bestScore = major;
            bestRoot = root;
            bestIsMinor = false;
        }
        if (minor > bestScore)
        {
            bestScore = minor;
            bestRoot = root;
            bestIsMinor = true;
        }
    }

    if (bestScore < 0.15)
        return {};
    return juce::String(keyNames[static_cast<std::size_t>(bestRoot)])
        + (bestIsMinor ? " minor" : " major");
}
}

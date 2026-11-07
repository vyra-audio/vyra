#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

namespace vyra::audio
{
class Biquad final
{
public:
    struct Coefficients
    {
        float b0 = 1.0f;
        float b1 = 0.0f;
        float b2 = 0.0f;
        float a1 = 0.0f;
        float a2 = 0.0f;
    };

    void setCoefficients(Coefficients value) noexcept
    {
        coefficients = value;
    }

    float process(float input) noexcept
    {
        const auto output = coefficients.b0 * input
            + coefficients.b1 * x1 + coefficients.b2 * x2
            - coefficients.a1 * y1 - coefficients.a2 * y2;
        x2 = x1;
        x1 = input;
        y2 = y1;
        y1 = std::isfinite(output) ? output : 0.0f;
        return y1;
    }

private:
    Coefficients coefficients;
    float x1 = 0.0f;
    float x2 = 0.0f;
    float y1 = 0.0f;
    float y2 = 0.0f;
};

class ThreeBandEqualizer final
{
public:
    void prepare(double sampleRate) noexcept
    {
        this->sampleRate = std::max(8000.0, sampleRate);
        lastLow = lastMid = lastHigh = 1000.0f;
    }

    void configure(float lowDb, float midDb, float highDb) noexcept
    {
        lowDb = std::clamp(lowDb, -12.0f, 12.0f);
        midDb = std::clamp(midDb, -12.0f, 12.0f);
        highDb = std::clamp(highDb, -12.0f, 12.0f);
        if (lowDb == lastLow && midDb == lastMid && highDb == lastHigh)
            return;

        const auto nyquistSafeRate = std::max(8000.0, sampleRate);
        for (auto& channel : channels)
        {
            channel.low.setCoefficients(lowShelf(250.0, lowDb, nyquistSafeRate));
            channel.mid.setCoefficients(peaking(1000.0, 0.8, midDb, nyquistSafeRate));
            channel.high.setCoefficients(highShelf(
                std::min(6000.0, nyquistSafeRate * 0.4), highDb, nyquistSafeRate));
        }

        lastLow = lowDb;
        lastMid = midDb;
        lastHigh = highDb;
    }

    float process(std::size_t channel, float input) noexcept
    {
        auto& equalizer = channels[std::min(channel, channels.size() - 1)];
        return equalizer.high.process(equalizer.mid.process(equalizer.low.process(input)));
    }

private:
    struct Channel
    {
        Biquad low;
        Biquad mid;
        Biquad high;
    };

    static Biquad::Coefficients lowShelf(double frequency, float gainDb,
                                         double sampleRate) noexcept
    {
        return shelf(frequency, gainDb, sampleRate, false);
    }

    static Biquad::Coefficients highShelf(double frequency, float gainDb,
                                          double sampleRate) noexcept
    {
        return shelf(frequency, gainDb, sampleRate, true);
    }

    static Biquad::Coefficients shelf(double frequency, float gainDb,
                                      double sampleRate, bool high) noexcept
    {
        const auto amplitude = std::pow(10.0, static_cast<double>(gainDb) / 40.0);
        const auto omega = 2.0 * std::numbers::pi * frequency / sampleRate;
        const auto cosine = std::cos(omega);
        const auto sine = std::sin(omega);
        const auto alpha = sine / std::sqrt(2.0);
        const auto rootAmplitude = std::sqrt(amplitude);
        double b0, b1, b2, a0, a1, a2;

        if (!high)
        {
            b0 = amplitude * ((amplitude + 1.0) - (amplitude - 1.0) * cosine
                              + 2.0 * rootAmplitude * alpha);
            b1 = 2.0 * amplitude * ((amplitude - 1.0) - (amplitude + 1.0) * cosine);
            b2 = amplitude * ((amplitude + 1.0) - (amplitude - 1.0) * cosine
                              - 2.0 * rootAmplitude * alpha);
            a0 = (amplitude + 1.0) + (amplitude - 1.0) * cosine
                 + 2.0 * rootAmplitude * alpha;
            a1 = -2.0 * ((amplitude - 1.0) + (amplitude + 1.0) * cosine);
            a2 = (amplitude + 1.0) + (amplitude - 1.0) * cosine
                 - 2.0 * rootAmplitude * alpha;
        }
        else
        {
            b0 = amplitude * ((amplitude + 1.0) + (amplitude - 1.0) * cosine
                              + 2.0 * rootAmplitude * alpha);
            b1 = -2.0 * amplitude * ((amplitude - 1.0) + (amplitude + 1.0) * cosine);
            b2 = amplitude * ((amplitude + 1.0) + (amplitude - 1.0) * cosine
                              - 2.0 * rootAmplitude * alpha);
            a0 = (amplitude + 1.0) - (amplitude - 1.0) * cosine
                 + 2.0 * rootAmplitude * alpha;
            a1 = 2.0 * ((amplitude - 1.0) - (amplitude + 1.0) * cosine);
            a2 = (amplitude + 1.0) - (amplitude - 1.0) * cosine
                 - 2.0 * rootAmplitude * alpha;
        }

        return normalise(b0, b1, b2, a0, a1, a2);
    }

    static Biquad::Coefficients peaking(double frequency, double quality,
                                        float gainDb, double sampleRate) noexcept
    {
        const auto amplitude = std::pow(10.0, static_cast<double>(gainDb) / 40.0);
        const auto omega = 2.0 * std::numbers::pi * frequency / sampleRate;
        const auto alpha = std::sin(omega) / (2.0 * quality);
        const auto cosine = std::cos(omega);

        return normalise(1.0 + alpha * amplitude, -2.0 * cosine,
                         1.0 - alpha * amplitude, 1.0 + alpha / amplitude,
                         -2.0 * cosine, 1.0 - alpha / amplitude);
    }

    static Biquad::Coefficients normalise(double b0, double b1, double b2,
                                          double a0, double a1, double a2) noexcept
    {
        if (!std::isfinite(a0) || std::abs(a0) < 1.0e-12)
            return {};

        const auto reciprocal = 1.0 / a0;
        return {
            static_cast<float>(b0 * reciprocal),
            static_cast<float>(b1 * reciprocal),
            static_cast<float>(b2 * reciprocal),
            static_cast<float>(a1 * reciprocal),
            static_cast<float>(a2 * reciprocal)
        };
    }

    std::array<Channel, 2> channels;
    double sampleRate = 44100.0;
    float lastLow = 1000.0f;
    float lastMid = 1000.0f;
    float lastHigh = 1000.0f;
};

class ChannelFilter final
{
public:
    void prepare(double rate) noexcept
    {
        sampleRate = std::max(8000.0, rate);
        state = 0.0f;
        previousCutoff = -1.0;
    }

    float process(float input, float amount) noexcept
    {
        amount = std::clamp(amount, -1.0f, 1.0f);
        if (std::abs(amount) < 0.001f)
        {
            state = input;
            return input;
        }

        const auto position = std::abs(static_cast<double>(amount));
        const auto cutoff = amount > 0.0f
            ? 18000.0 * std::pow(50.0 / 18000.0, position)
            : 40.0 * std::pow(6000.0 / 40.0, position);
        if (std::abs(cutoff - previousCutoff) > 0.01)
        {
            coefficient = static_cast<float>(1.0 - std::exp(
                -2.0 * std::numbers::pi * std::min(cutoff, sampleRate * 0.45) / sampleRate));
            previousCutoff = cutoff;
        }

        state += coefficient * (input - state);
        return amount > 0.0f ? state : input - state;
    }

private:
    double sampleRate = 44100.0;
    double previousCutoff = -1.0;
    float coefficient = 0.0f;
    float state = 0.0f;
};

class EchoEffect final
{
public:
    void prepare(double sampleRate)
    {
        const auto delaySamples = std::clamp(
            static_cast<int>(std::round(sampleRate * 0.375)), 1, 96000);
        for (auto& line : delay)
            line.assign(static_cast<std::size_t>(delaySamples), 0.0f);
        writePosition = 0;
    }

    float process(std::size_t channel, float input, float wet) noexcept
    {
        auto& line = delay[std::min(channel, delay.size() - 1)];
        if (line.empty())
            return input;

        const auto delayed = line[writePosition];
        line[writePosition] = input + delayed * 0.35f;

        wet = std::clamp(wet, 0.0f, 0.8f);
        return input * (1.0f - wet) + delayed * wet;
    }

    void advance() noexcept
    {
        if (delay[0].empty())
            return;

        ++writePosition;
        if (writePosition == delay[0].size())
            writePosition = 0;
    }

private:
    std::array<std::vector<float>, 2> delay;
    std::size_t writePosition = 0;
};
}

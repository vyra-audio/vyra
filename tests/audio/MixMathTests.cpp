#include "vyra/audio/MixMath.h"
#include "vyra/audio/ChannelDsp.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <numbers>

namespace
{
void expectNear(float actual, float expected, float tolerance, const char* message)
{
    if (std::abs(actual - expected) > tolerance)
    {
        std::cerr << message << ": expected " << expected << ", got " << actual << '\n';
        std::exit(EXIT_FAILURE);
    }
}
}

int main()
{
    using vyra::audio::equalPowerCrossfade;
    using vyra::audio::limiterTargetGain;

    const auto left = equalPowerCrossfade(0.0f);
    expectNear(left.first, 1.0f, 1.0e-6f, "crossfade left endpoint");
    expectNear(left.second, 0.0f, 1.0e-6f, "crossfade left endpoint");

    const auto center = equalPowerCrossfade(0.5f);
    expectNear(center.first, std::sqrt(0.5f), 1.0e-6f, "crossfade center left");
    expectNear(center.second, std::sqrt(0.5f), 1.0e-6f, "crossfade center right");
    expectNear(center.first * center.first + center.second * center.second,
               1.0f, 1.0e-6f, "crossfade power");

    const auto right = equalPowerCrossfade(1.0f);
    expectNear(right.first, 0.0f, 1.0e-6f, "crossfade right endpoint");
    expectNear(right.second, 1.0f, 1.0e-6f, "crossfade right endpoint");
    const auto clamped = equalPowerCrossfade(2.0f);
    expectNear(clamped.second, 1.0f, 1.0e-6f, "crossfade input clamping");

    expectNear(limiterTargetGain(0.5f, 0.98f), 1.0f, 1.0e-6f, "limiter below ceiling");
    expectNear(limiterTargetGain(2.0f, 1.0f), 0.5f, 1.0e-6f, "limiter reduction");
    expectNear(limiterTargetGain(1.0f, 0.0f), 0.0f, 1.0e-6f, "invalid limiter ceiling");
    expectNear(static_cast<float>(vyra::audio::beatJumpPosition(10.0, 100.0, 120.0, 4)),
               12.0f, 1.0e-6f, "four-beat forward jump");
    expectNear(static_cast<float>(vyra::audio::beatJumpPosition(1.0, 100.0, 120.0, -4)),
               0.0f, 1.0e-6f, "beat jump clamps at track start");
    expectNear(static_cast<float>(vyra::audio::beatJumpPosition(99.0, 100.0, 120.0, 4)),
               100.0f, 1.0e-6f, "beat jump clamps at track end");
    expectNear(static_cast<float>(vyra::audio::beatJumpPosition(10.0, 100.0, 0.0, 4)),
               10.0f, 1.0e-6f, "beat jump ignores unavailable tempo");
    expectNear(static_cast<float>(vyra::audio::pitchRatioFromPercent(8.0f)),
               1.08f, 1.0e-6f, "pitch percentage converts to playback ratio");
    expectNear(vyra::audio::tempoSyncPitchPercent(100.0, 110.0),
               10.0f, 1.0e-5f, "tempo sync computes target pitch");
    expectNear(vyra::audio::tempoSyncPitchPercent(100.0, 130.0),
               16.0f, 1.0e-5f, "tempo sync is bounded to pitch range");
    expectNear(vyra::audio::tempoSyncPitchPercent(0.0, 120.0),
               0.0f, 1.0e-6f, "tempo sync rejects unavailable BPM");
    expectNear(vyra::audio::headphoneMonitorSample(0.8f, 0.2f, 0.0f),
               0.8f, 1.0e-6f, "headphone cue-only mix");
    expectNear(vyra::audio::headphoneMonitorSample(0.8f, 0.2f, 1.0f),
               0.2f, 1.0e-6f, "headphone master-only mix");
    expectNear(vyra::audio::headphoneMonitorSample(0.8f, 0.2f, 0.5f),
               0.5f, 1.0e-6f, "headphone cue/master blend");

    vyra::audio::ThreeBandEqualizer equalizer;
    equalizer.prepare(48000.0);
    equalizer.configure(0.0f, 0.0f, 0.0f);
    for (int sample = 0; sample < 100; ++sample)
        expectNear(equalizer.process(0, sample == 0 ? 1.0f : 0.0f),
                   sample == 0 ? 1.0f : 0.0f, 1.0e-5f, "flat equalizer");

    equalizer.configure(-12.0f, -12.0f, -12.0f);
    float processedEnergy = 0.0f;
    for (int sample = 0; sample < 24000; ++sample)
    {
        const auto input = std::sin(2.0f * std::numbers::pi_v<float> * 1000.0f
                                    * static_cast<float>(sample) / 48000.0f);
        const auto output = equalizer.process(0, input);
        if (sample > 10000)
            processedEnergy += output * output;
    }
    if (!(processedEnergy < 7000.0f))
    {
        std::cerr << "equalizer cut should attenuate the mid band\n";
        return EXIT_FAILURE;
    }

    vyra::audio::EchoEffect echo;
    echo.prepare(1000.0);
    float firstEcho = 0.0f;
    float repeatedEcho = 0.0f;
    for (int sample = 0; sample <= 750; ++sample)
    {
        const auto input = sample == 0 ? 1.0f : 0.0f;
        const auto output = echo.process(0, input, 0.5f);
        if (sample == 375)
            firstEcho = output;
        if (sample == 750)
            repeatedEcho = output;
        echo.process(1, input, 0.5f);
        echo.advance();
    }
    expectNear(firstEcho, 0.5f, 1.0e-5f, "echo delay");
    expectNear(repeatedEcho, 0.175f, 1.0e-5f, "echo feedback");

    std::cout << "Mix math tests passed\n";
    return EXIT_SUCCESS;
}

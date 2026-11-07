#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

namespace vyra::audio
{
inline std::pair<float, float> equalPowerCrossfade(float position) noexcept
{
    const auto clamped = std::clamp(position, 0.0f, 1.0f);
    const auto angle = clamped * std::numbers::pi_v<float> * 0.5f;
    return { std::cos(angle), std::sin(angle) };
}

inline float limiterTargetGain(float peak, float ceiling) noexcept
{
    if (!std::isfinite(peak) || !std::isfinite(ceiling) || ceiling <= 0.0f)
        return 0.0f;

    return peak <= ceiling ? 1.0f : ceiling / peak;
}

inline double beatJumpPosition(double position, double length,
                               double bpm, int beats) noexcept
{
    if (!std::isfinite(position))
        position = 0.0;
    if (!std::isfinite(length) || length <= 0.0)
        return std::max(0.0, position);
    if (!std::isfinite(bpm) || bpm <= 0.0)
        return std::clamp(position, 0.0, length);

    return std::clamp(position + static_cast<double>(beats) * 60.0 / bpm,
                      0.0, length);
}

inline double pitchRatioFromPercent(float percent) noexcept
{
    if (!std::isfinite(percent))
        return 1.0;
    return 1.0 + static_cast<double>(std::clamp(percent, -16.0f, 16.0f)) / 100.0;
}

inline float tempoSyncPitchPercent(double targetBpm,
                                   double referenceBpm) noexcept
{
    if (!std::isfinite(targetBpm) || !std::isfinite(referenceBpm)
        || targetBpm <= 0.0 || referenceBpm <= 0.0)
        return 0.0f;
    const auto ratio = std::clamp(referenceBpm / targetBpm, 0.84, 1.16);
    return static_cast<float>((ratio - 1.0) * 100.0);
}

inline float headphoneMonitorSample(float cueSignal, float masterSignal,
                                    float mix) noexcept
{
    const auto boundedMix = std::clamp(mix, 0.0f, 1.0f);
    return cueSignal * (1.0f - boundedMix) + masterSignal * boundedMix;
}
}

#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include <cstdint>
#include <vector>

namespace vyra::audio
{
[[nodiscard]] std::vector<std::uint8_t> analyzeWaveform(
    juce::AudioFormatReader& reader, int binCount = 512);
}

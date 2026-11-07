#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

namespace vyra::audio
{
[[nodiscard]] juce::String analyzeMusicalKey(juce::AudioFormatReader& reader);
}

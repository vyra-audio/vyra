#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

namespace vyra::audio
{
[[nodiscard]] double analyzeBpm(juce::AudioFormatReader& reader);
}

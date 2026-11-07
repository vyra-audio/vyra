#include "vyra/audio/LoopRegionAudioSource.h"

#include <cstdlib>
#include <iostream>
#include <memory>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
}

int main()
{
    constexpr double sampleRate = 8000.0;
    const auto file = juce::File::getSpecialLocation(
        juce::File::tempDirectory).getChildFile("vyra-loop-region-test.wav");
    file.deleteFile();

    juce::WavAudioFormat format;
    auto stream = file.createOutputStream();
    require(stream != nullptr, "loop test WAV should open");
    auto* rawWriter = format.createWriterFor(stream.get(), sampleRate, 1, 16, {}, 0);
    require(rawWriter != nullptr, "loop test writer should initialize");
    stream.release();
    std::unique_ptr<juce::AudioFormatWriter> writer(rawWriter);

    juce::AudioBuffer<float> samples(1, 8);
    for (int i = 0; i < samples.getNumSamples(); ++i)
        samples.setSample(0, i, static_cast<float>(i + 1) / 10.0f);
    require(writer->writeFromAudioSampleBuffer(samples, 0, samples.getNumSamples()),
            "loop test samples should be written");
    writer.reset();

    auto inputStream = file.createInputStream();
    require(inputStream != nullptr, "loop test input stream should open");
    std::unique_ptr<juce::AudioFormatReader> reader(
        format.createReaderFor(inputStream.release(), true));
    require(reader != nullptr, "loop test file should decode");
    juce::AudioFormatReaderSource readerSource(reader.release(), true);
    vyra::audio::LoopRegionAudioSource loopSource(readerSource);
    loopSource.prepareToPlay(16, sampleRate);
    loopSource.setLoopRegion(2.0 / sampleRate, 6.0 / sampleRate);
    loopSource.setNextReadPosition(2);

    juce::AudioBuffer<float> output(1, 10);
    output.clear();
    loopSource.getNextAudioBlock({ &output, 0, output.getNumSamples() });
    for (int sample = 0; sample < output.getNumSamples(); ++sample)
    {
        const auto expected = static_cast<float>((sample % 4) + 3) / 10.0f;
        if (std::abs(output.getSample(0, sample) - expected) > 0.0001f)
        {
            std::cerr << "FAIL: loop should wrap cleanly at its configured region\n";
            return EXIT_FAILURE;
        }
    }

    loopSource.clearLoop();
    require(!loopSource.isRegionLooping(), "loop can be disabled");
    loopSource.releaseResources();
    file.deleteFile();
    std::cout << "Loop region tests passed\n";
    return EXIT_SUCCESS;
}

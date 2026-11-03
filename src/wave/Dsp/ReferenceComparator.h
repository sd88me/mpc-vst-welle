#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace wave::dsp
{
class ReferenceComparator
{
public:
    struct Metrics
    {
        int lagSamples = 0;
        float fittedGain = 1.0f;
        float correlation = 0.0f;
        float rmsError = 0.0f;
        float peakError = 0.0f;
    };

    [[nodiscard]] static Metrics compare(const juce::AudioBuffer<float>& reference,
                                         const juce::AudioBuffer<float>& candidate,
                                         int maximumLagSamples = 2048) noexcept;
};
} // namespace wave::dsp

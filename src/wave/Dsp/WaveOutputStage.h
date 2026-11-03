#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <cstdint>

namespace wave::dsp
{
class WaveOutputStage
{
public:
    void prepare(double newSampleRate) noexcept;
    void reset() noexcept;
    [[nodiscard]] std::array<float, 2> process(float left, float right,
                                               float circuitAge) noexcept;

private:
    void updateAge(float circuitAge) noexcept;

    double sampleRate = 44100.0;
    std::array<float, 2> previousInput{};
    std::array<float, 2> highPassState{};
    std::array<float, 2> slewState{};
    uint32_t noiseState = 0x6d2b79f5u;
    float age = -1.0f;
    float highPassCoefficient = 0.0f;
    float maximumStep = 0.0f;
    float saturationGain = 0.82f;
    float noiseGain = 0.0000015f;
};
} // namespace wave::dsp

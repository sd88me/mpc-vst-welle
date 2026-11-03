#include "WaveOutputStage.h"

#include <cmath>

namespace wave::dsp
{
void WaveOutputStage::prepare(double newSampleRate) noexcept
{
    sampleRate = juce::jmax(1.0, newSampleRate);
    reset();
}

void WaveOutputStage::reset() noexcept
{
    previousInput.fill(0.0f);
    highPassState.fill(0.0f);
    slewState.fill(0.0f);
    noiseState = 0x6d2b79f5u;
    age = -1.0f;
    updateAge(0.0f);
}

void WaveOutputStage::updateAge(float circuitAge) noexcept
{
    const auto limited = juce::jlimit(0.0f, 1.0f, circuitAge);
    if (limited == age)
        return;
    age = limited;
    highPassCoefficient = std::exp(-juce::MathConstants<float>::twoPi * 3.2f
                                   / static_cast<float>(sampleRate));
    maximumStep = (320000.0f - age * 80000.0f) / static_cast<float>(sampleRate);
    saturationGain = 0.82f + age * 0.12f;
    noiseGain = 0.0000015f + age * 0.000004f;
}

std::array<float, 2> WaveOutputStage::process(float left, float right,
                                              float circuitAge) noexcept
{
    std::array<float, 2> result{};
    const std::array<float, 2> input { left, right };
    updateAge(circuitAge);

    for (size_t channel = 0; channel < result.size(); ++channel)
    {
        const auto coupled = input[channel] - previousInput[channel]
                             + highPassCoefficient * highPassState[channel];
        previousInput[channel] = input[channel];
        highPassState[channel] = coupled;

        const auto target = std::tanh(coupled * saturationGain);
        slewState[channel] += juce::jlimit(-maximumStep, maximumStep,
                                           target - slewState[channel]);

        noiseState ^= noiseState << 13u;
        noiseState ^= noiseState >> 17u;
        noiseState ^= noiseState << 5u;
        const auto noise = static_cast<float>(static_cast<int32_t>(noiseState))
                           / 2147483648.0f * noiseGain;
        result[channel] = slewState[channel] + noise;
    }
    return result;
}
} // namespace wave::dsp

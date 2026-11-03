#include "WaveLfo.h"

#include <juce_core/juce_core.h>

#include <cmath>

namespace wave::dsp
{
void WaveLfo::prepare(double newSampleRate, uint32_t seed) noexcept
{
    sampleRate = juce::jmax(1.0, newSampleRate);
    initialSeed = seed != 0 ? seed : 0x12345678u;
    reset();
}

void WaveLfo::reset() noexcept
{
    phase = 0.0;
    rateScale = 1.0;
    randomState = initialSeed;
    randomPrevious = randomBipolar();
    randomCurrent = randomBipolar();
    output = 0.0f;
    cachedRawRate = -1.0f;
    cachedRateHz = 0.09;
}

void WaveLfo::noteOn(int syncMode, float phaseDegrees) noexcept
{
    // Off leaves the oscillator free-running. Sync and Retrig both establish
    // a deterministic note boundary; Sync retains the programmed phase while
    // Retrig begins a fresh random sequence as well.
    if (syncMode <= 0)
        return;

    phase = std::fmod(juce::jlimit(0.0, 360.0, static_cast<double>(phaseDegrees)) / 360.0,
                      1.0);
    if (syncMode >= 2)
    {
        randomState = initialSeed;
        randomPrevious = randomBipolar();
        randomCurrent = randomBipolar();
        rateScale = 1.0;
    }
}

double WaveLfo::rateHz(float rawRate) noexcept
{
    const auto normalised = static_cast<double>(juce::jlimit(0.0f, 127.0f, rawRate))
                            / 127.0;
    return 0.09 * std::pow(24.0 / 0.09, normalised);
}

float WaveLfo::process(float rawRate, int shape, float symmetry, int humanize) noexcept
{
    advancePhase(1, rawRate, humanize);
    return renderShape(shape, symmetry);
}

float WaveLfo::processSamples(int samples, float rawRate, int shape,
                              float symmetry, int humanize) noexcept
{
    advancePhase(samples, rawRate, humanize);
    return renderShape(shape, symmetry);
}

void WaveLfo::advancePhase(int samples, float rawRate, int humanize) noexcept
{
    if (samples <= 0)
        return;
    if (rawRate != cachedRawRate)
    {
        cachedRawRate = rawRate;
        cachedRateHz = rateHz(rawRate);
    }

    auto remaining = samples;
    while (remaining > 0)
    {
        const auto increment = cachedRateHz * rateScale / sampleRate;
        const auto untilWrap = juce::jmax(
            1, static_cast<int>(std::ceil((1.0 - phase) / increment)));
        const auto chunk = juce::jmin(remaining, untilWrap);
        phase += increment * static_cast<double>(chunk);
        remaining -= chunk;
        if (phase >= 1.0)
        {
            phase -= std::floor(phase);
            randomPrevious = randomCurrent;
            randomCurrent = randomBipolar();
            beginNextCycle(humanize);
        }
    }
}

float WaveLfo::renderShape(int shape, float symmetry) noexcept
{
    const auto symmetryAmount = juce::jlimit(-64.0, 63.0,
                                              static_cast<double>(symmetry));
    const auto split = juce::jlimit(0.05, 0.95, 0.5 + symmetryAmount / 140.0);
    const auto shapedPhase = phase < split
                                 ? phase * (0.5 / split)
                                 : 0.5 + (phase - split) * (0.5 / (1.0 - split));

    switch (juce::jlimit(0, 5, shape))
    {
        case 0:
            output = static_cast<float>(std::sin(
                juce::MathConstants<double>::twoPi * shapedPhase));
            break;
        case 1:
            output = static_cast<float>(1.0 - 4.0 * std::abs(shapedPhase - 0.5));
            break;
        case 2:
            output = static_cast<float>(2.0 * shapedPhase - 1.0);
            break;
        case 3:
            output = phase < split ? 1.0f : -1.0f;
            break;
        case 4:
        {
            const auto blend = static_cast<float>(0.5 - 0.5 * std::cos(
                juce::MathConstants<double>::pi * shapedPhase));
            output = randomPrevious + (randomCurrent - randomPrevious) * blend;
            break;
        }
        case 5:
            output = randomCurrent;
            break;
        default:
            break;
    }
    return output;
}

float WaveLfo::randomBipolar() noexcept
{
    randomState ^= randomState << 13u;
    randomState ^= randomState >> 17u;
    randomState ^= randomState << 5u;
    return static_cast<float>(static_cast<int32_t>(randomState)) / 2147483648.0f;
}

void WaveLfo::beginNextCycle(int humanize) noexcept
{
    const auto depth = static_cast<float>(juce::jlimit(0, 7, humanize)) / 7.0f;
    rateScale = 1.0 + static_cast<double>(randomBipolar() * depth * 0.075f);
}
} // namespace wave::dsp

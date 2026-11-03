#pragma once

#include <cstdint>

namespace wave::dsp
{
class WaveLfo
{
public:
    void prepare(double sampleRate, uint32_t seed) noexcept;
    void reset() noexcept;
    void noteOn(int syncMode, float phaseDegrees) noexcept;
    [[nodiscard]] float process(float rawRate, int shape, float symmetry,
                                int humanize) noexcept;
    [[nodiscard]] float processSamples(int samples, float rawRate, int shape,
                                       float symmetry, int humanize) noexcept;

    [[nodiscard]] static double rateHz(float rawRate) noexcept;
    [[nodiscard]] float currentValue() const noexcept { return output; }

private:
    [[nodiscard]] float randomBipolar() noexcept;
    void beginNextCycle(int humanize) noexcept;
    void advancePhase(int samples, float rawRate, int humanize) noexcept;
    [[nodiscard]] float renderShape(int shape, float symmetry) noexcept;

    double sampleRate = 44100.0;
    double phase = 0.0;
    double rateScale = 1.0;
    uint32_t initialSeed = 0x12345678u;
    uint32_t randomState = 0x12345678u;
    float randomPrevious = 0.0f;
    float randomCurrent = 0.0f;
    float output = 0.0f;
    float cachedRawRate = -1.0f;
    double cachedRateHz = 0.09;
};
} // namespace wave::dsp

#pragma once

#include <array>
#include <cstdint>

namespace wave::dsp
{
class WdvEnvelope final
{
public:
    struct Parameters
    {
        float attack = 0.001f;
        float decay = 0.1f;
        float sustain = 1.0f;
        float release = 0.1f;
        bool useMeasuredAmplifierAttackScaling = false;
    };

    void prepare(double newSampleRate) noexcept;
    void reset() noexcept;
    void setParameters(const Parameters& newParameters) noexcept;
    void noteOn() noexcept;
    void noteOff() noexcept;
    [[nodiscard]] float getNextSample() noexcept;
    [[nodiscard]] bool isActive() const noexcept { return stage != Stage::idle; }

    [[nodiscard]] static float timeConstantForRate(uint8_t rate) noexcept;
    [[nodiscard]] static float amplifierAttackTimeConstantForRate(uint8_t rate) noexcept;

private:
    enum class Stage : uint8_t { idle, attack, decay, sustain, release };
    void advanceControlTick() noexcept;
    void advanceFastAmplifierAttack() noexcept;
    [[nodiscard]] static float coefficient(float timeConstant) noexcept;
    [[nodiscard]] float audioRateCoefficient(float timeConstant) const noexcept;

    Parameters parameters;
    double sampleRate = 44100.0;
    double samplesUntilControlTick = 0.0;
    float level = 0.0f;
    Stage stage = Stage::idle;

    static constexpr float controlRateHz = 53.0f;
    static constexpr float completionThreshold = 1.0f / 127.0f;
    static constexpr float instantAttackSentinel = 0.001f;
    static constexpr float fastestSlewedAttackSeconds = 0.004f;
};
} // namespace wave::dsp

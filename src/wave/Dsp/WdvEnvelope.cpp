#include "WdvEnvelope.h"

#include <algorithm>
#include <cmath>

namespace wave::dsp
{
namespace
{
// Exact 128-word rate table at $001778 in Waldorf WDV.SYS 1.700.
constexpr std::array<uint16_t, 128> wdvEnvelopeRates {
    32767, 32767, 30500, 28500, 26500, 24800, 23100, 21600,
    20100, 18800, 17500, 16400, 15300, 14250, 13300, 12400,
    11570, 10800, 10000, 9400, 8800, 8200, 7600, 7100,
    6600, 6200, 5800, 5400, 5000, 4700, 4400, 4100,
    3800, 3500, 3300, 3100, 2900, 2700, 2500, 2350,
    2200, 2050, 1900, 1780, 1660, 1550, 1450, 1350,
    1260, 1170, 1090, 1020, 950, 890, 830, 775,
    720, 670, 630, 590, 550, 510, 480, 445,
    415, 385, 361, 335, 314, 293, 274, 255,
    238, 222, 207, 193, 180, 168, 157, 146,
    137, 127, 119, 111, 103, 97, 90, 84,
    79, 73, 68, 64, 60, 56, 52, 48,
    45, 42, 39, 37, 34, 32, 30, 28,
    26, 24, 23, 21, 20, 18, 17, 16,
    15, 14, 13, 12, 11, 10, 10, 9,
    9, 8, 7, 7, 6, 6, 6, 5
};
} // namespace

void WdvEnvelope::prepare(double newSampleRate) noexcept
{
    sampleRate = std::max(1.0, newSampleRate);
    reset();
}

void WdvEnvelope::reset() noexcept
{
    level = 0.0f;
    stage = Stage::idle;
    samplesUntilControlTick = 0.0;
}

void WdvEnvelope::setParameters(const Parameters& newParameters) noexcept
{
    parameters.attack = std::max(1.0e-6f, newParameters.attack);
    parameters.decay = std::max(1.0e-6f, newParameters.decay);
    parameters.sustain = std::clamp(newParameters.sustain, 0.0f, 1.0f);
    parameters.release = std::max(1.0e-6f, newParameters.release);
    parameters.useMeasuredAmplifierAttackScaling
        = newParameters.useMeasuredAmplifierAttackScaling;
}

void WdvEnvelope::noteOn() noexcept
{
    stage = Stage::attack;
    samplesUntilControlTick = 0.0;
}

void WdvEnvelope::noteOff() noexcept
{
    if (stage != Stage::idle)
    {
        stage = Stage::release;
        samplesUntilControlTick = 0.0;
    }
}

float WdvEnvelope::getNextSample() noexcept
{
    if (stage == Stage::idle)
        return 0.0f;

    // WDV rate zero is the hardware's deliberately abrupt, click-prone VCA
    // attack. Rate one uses the same near-unity firmware table coefficient,
    // but the measured Wave output takes about 5 ms to rise. Model that short
    // analogue acquisition interval between firmware control ticks; applying
    // the coefficient only at 53 Hz incorrectly makes both settings instant.
    if (stage == Stage::attack
        && parameters.useMeasuredAmplifierAttackScaling
        && parameters.attack <= fastestSlewedAttackSeconds)
    {
        advanceFastAmplifierAttack();
        return level;
    }

    if (samplesUntilControlTick <= 0.0)
    {
        advanceControlTick();
        samplesUntilControlTick += sampleRate / static_cast<double>(controlRateHz);
    }
    samplesUntilControlTick -= 1.0;
    return level;
}

void WdvEnvelope::advanceFastAmplifierAttack() noexcept
{
    if (parameters.attack <= instantAttackSentinel)
        level = 1.0f;
    else
        level += (1.0f - level) * audioRateCoefficient(parameters.attack);

    if (1.0f - level <= completionThreshold)
    {
        level = 1.0f;
        stage = Stage::decay;
        samplesUntilControlTick = sampleRate / static_cast<double>(controlRateHz);
    }
}

void WdvEnvelope::advanceControlTick() noexcept
{
    switch (stage)
    {
        case Stage::attack:
            level += (1.0f - level) * coefficient(parameters.attack);
            if (1.0f - level <= completionThreshold)
                stage = Stage::decay;
            break;
        case Stage::decay:
            level += (parameters.sustain - level) * coefficient(parameters.decay);
            if (std::abs(parameters.sustain - level) <= completionThreshold)
            {
                level = parameters.sustain;
                stage = Stage::sustain;
            }
            break;
        case Stage::sustain:
            level = parameters.sustain;
            break;
        case Stage::release:
            level += (0.0f - level) * coefficient(parameters.release);
            if (level <= completionThreshold)
                reset();
            break;
        case Stage::idle:
            break;
    }
}

float WdvEnvelope::coefficient(float timeConstant) noexcept
{
    return 1.0f - std::exp(-1.0f / (controlRateHz * std::max(1.0e-6f, timeConstant)));
}

float WdvEnvelope::audioRateCoefficient(float timeConstant) const noexcept
{
    return 1.0f - std::exp(
                      -1.0f / (static_cast<float>(sampleRate)
                               * std::max(1.0e-6f, timeConstant)));
}

float WdvEnvelope::timeConstantForRate(uint8_t rate) noexcept
{
    const auto tableCoefficient = static_cast<float>(wdvEnvelopeRates[rate & 0x7fu]) / 32768.0f;
    return -1.0f / (controlRateHz * std::log(std::max(1.0e-9f, 1.0f - tableCoefficient)));
}

float WdvEnvelope::amplifierAttackTimeConstantForRate(uint8_t rate) noexcept
{
    const auto bounded = static_cast<uint8_t>(rate & 0x7fu);
    if (bounded == 0)
        return instantAttackSentinel;
    if (bounded == 1)
    {
        // The CEM path converts the envelope CV through the modelled VCA
        // transfer curve. These are the CV points at 10% and 90% output gain;
        // choose tau so the audible 10%-90% rise is the measured 5 ms.
        constexpr auto cvAtTenPercentGain = 0.1958001f;
        constexpr auto cvAtNinetyPercentGain = 0.8041999f;
        return 0.005f
               / std::log((1.0f - cvAtTenPercentGain)
                          / (1.0f - cvAtNinetyPercentGain));
    }
    return timeConstantForRate(bounded);
}
} // namespace wave::dsp

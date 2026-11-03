#pragma once

#include "../WaveParameters.h"

namespace wave::dsp
{
class WaveEnvelope
{
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void noteOn() noexcept;
    void noteOff() noexcept;
    [[nodiscard]] float process(const parameters::Snapshot& parameters) noexcept;
    [[nodiscard]] float currentValue() const noexcept { return value; }

    [[nodiscard]] static double segmentTimeSeconds(float rawTime) noexcept;

private:
    void completeSegment(const parameters::Snapshot& parameters) noexcept;

    double sampleRate = 44100.0;
    float value = 0.0f;
    float target = 0.0f;
    float increment = 0.0f;
    int segment = 0;
    int samplesRemaining = 0;
    bool keyDown = false;
    bool sustaining = false;
    bool needsSegment = true;
    bool running = false;
};

// Four-stage bipolar control envelope used by the Wave's freely assignable
// envelope and by the Dual-filter high-pass envelope selector.
class FreeEnvelope
{
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void noteOn() noexcept;
    [[nodiscard]] float process(const parameters::Snapshot& parameters) noexcept;

private:
    double sampleRate = 44100.0;
    float value = 0.0f;
    float target = 0.0f;
    float increment = 0.0f;
    int segment = 0;
    int samplesRemaining = 0;
    bool needsSegment = true;
    bool running = false;
};
} // namespace wave::dsp

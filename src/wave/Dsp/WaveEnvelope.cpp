#include "WaveEnvelope.h"

#include <cmath>

namespace wave::dsp
{
void WaveEnvelope::prepare(double newSampleRate) noexcept
{
    sampleRate = juce::jmax(1.0, newSampleRate);
    reset();
}

void WaveEnvelope::reset() noexcept
{
    value = 0.0f;
    target = 0.0f;
    increment = 0.0f;
    segment = 0;
    samplesRemaining = 0;
    keyDown = false;
    sustaining = false;
    needsSegment = true;
    running = false;
}

void WaveEnvelope::noteOn() noexcept
{
    value = 0.0f;
    target = 0.0f;
    increment = 0.0f;
    segment = 0;
    samplesRemaining = 0;
    keyDown = true;
    sustaining = false;
    needsSegment = true;
    running = true;
}

void WaveEnvelope::noteOff() noexcept
{
    keyDown = false;
}

double WaveEnvelope::segmentTimeSeconds(float rawTime) noexcept
{
    if (rawTime <= 0.0f)
        return 0.0;

    // The ASIC's exact rate table is undocumented. This cubic approximation
    // gives the Wave's useful fast lower range and its minute-long upper range
    // while retaining every genuine 0..127 factory time value.
    const auto normalised = static_cast<double>(juce::jlimit(0.0f, 127.0f, rawTime)) / 127.0;
    return 60.0 * normalised * normalised * normalised;
}

float WaveEnvelope::process(const parameters::Snapshot& parameters) noexcept
{
    if (!running)
        return value;

    const auto keyOffPoint = juce::jlimit(0, 7, parameters.waveEnvelopeKeyOffPoint);

    for (int instantSegmentGuard = 0; instantSegmentGuard < 16; ++instantSegmentGuard)
    {
        if (sustaining)
        {
            if (keyDown)
                return value;
            sustaining = false;
            segment = keyOffPoint + 1;
            needsSegment = true;
        }

        if (segment >= 8)
        {
            running = false;
            return value;
        }

        if (needsSegment)
        {
            target = juce::jlimit(0.0f, 1.0f,
                                  parameters.waveEnvelopeLevels[static_cast<size_t>(segment)]
                                      / 127.0f);
            samplesRemaining = juce::roundToInt(
                segmentTimeSeconds(
                    parameters.waveEnvelopeTimes[static_cast<size_t>(segment)])
                * sampleRate);
            if (samplesRemaining <= 0)
            {
                value = target;
                completeSegment(parameters);
                continue;
            }
            increment = (target - value) / static_cast<float>(samplesRemaining);
            needsSegment = false;
        }

        value += increment;
        if (--samplesRemaining <= 0)
        {
            value = target;
            completeSegment(parameters);
        }
        return value;
    }

    return value;
}

void WaveEnvelope::completeSegment(const parameters::Snapshot& parameters) noexcept
{
    const auto keyOffPoint = juce::jlimit(0, 7, parameters.waveEnvelopeKeyOffPoint);
    const auto loopStart = juce::jlimit(0, 7, parameters.waveEnvelopeLoopStartPoint);
    needsSegment = true;

    if (segment == keyOffPoint && keyDown)
    {
        if (parameters.waveEnvelopeLoop && loopStart < keyOffPoint)
        {
            value = juce::jlimit(
                0.0f, 1.0f,
                parameters.waveEnvelopeLevels[static_cast<size_t>(loopStart)] / 127.0f);
            segment = loopStart + 1;
        }
        else
        {
            sustaining = true;
            segment = keyOffPoint + 1;
        }
        return;
    }

    if (!keyDown && parameters.waveEnvelopeLoop && loopStart > keyOffPoint
        && segment == loopStart)
    {
        value = juce::jlimit(
            0.0f, 1.0f,
            parameters.waveEnvelopeLevels[static_cast<size_t>(keyOffPoint)] / 127.0f);
        segment = keyOffPoint + 1;
        return;
    }

    ++segment;
    if (segment >= 8)
        running = false;
}

void FreeEnvelope::prepare(double newSampleRate) noexcept
{
    sampleRate = juce::jmax(1.0, newSampleRate);
    reset();
}

void FreeEnvelope::reset() noexcept
{
    value = 0.0f;
    target = 0.0f;
    increment = 0.0f;
    segment = 0;
    samplesRemaining = 0;
    needsSegment = true;
    running = false;
}

void FreeEnvelope::noteOn() noexcept
{
    reset();
    running = true;
}

float FreeEnvelope::process(const parameters::Snapshot& parameters) noexcept
{
    if (!running)
        return value;

    for (int instantSegmentGuard = 0; instantSegmentGuard < 8; ++instantSegmentGuard)
    {
        if (segment >= 4)
        {
            running = false;
            return value;
        }

        if (needsSegment)
        {
            const auto level = parameters.freeEnvelopeLevels[static_cast<size_t>(segment)];
            target = juce::jlimit(-1.0f, 1.0f,
                                  (level - parameters.freeEnvelopeZeroAxis) / 64.0f);
            samplesRemaining = juce::roundToInt(
                WaveEnvelope::segmentTimeSeconds(
                    parameters.freeEnvelopeTimes[static_cast<size_t>(segment)])
                * sampleRate);
            if (samplesRemaining <= 0)
            {
                value = target;
                ++segment;
                continue;
            }
            increment = (target - value) / static_cast<float>(samplesRemaining);
            needsSegment = false;
        }

        value += increment;
        if (--samplesRemaining <= 0)
        {
            value = target;
            ++segment;
            needsSegment = true;
        }
        return value;
    }

    return value;
}
} // namespace wave::dsp

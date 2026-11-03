#include "WaveParameters.h"

#include <cmath>

namespace wave::parameters
{
float cutoffFrequencyForStep(float step) noexcept
{
    return 20.0f * std::exp2(juce::jlimit(0.0f, 127.0f, step) / 12.0f);
}

float cutoffStepForFrequency(float frequencyHz) noexcept
{
    return juce::jlimit(0.0f, 127.0f,
                        12.0f * std::log2(juce::jmax(20.0f, frequencyHz) / 20.0f));
}

namespace
{
float read(const juce::AudioProcessorValueTreeState& state, const char* id)
{
    if (const auto* value = state.getRawParameterValue(id))
        return value->load(std::memory_order_relaxed);

    jassertfalse;
    return 0.0f;
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using Float = juce::AudioParameterFloat;
    using Int = juce::AudioParameterInt;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const auto makeFloat = [](const char* id, juce::String name,
                              juce::NormalisableRange<float> range, float defaultValue,
                              const char* label = "") {
        const juce::String parameterName(id);
        const auto formatValue = [parameterName](float value, int) {
            if (parameterName == cutoff)
                return juce::String(juce::roundToInt(value));
            if (parameterName == attack || parameterName == decay || parameterName == release
                || parameterName == filterDelay || parameterName == filterAttack
                || parameterName == filterDecay || parameterName == filterRelease)
                return juce::String(value, value < 0.1f ? 3 : 2);
            if (parameterName == position || parameterName == scan || parameterName == detune
                || parameterName == filterEnv || parameterName == drive || parameterName == output)
                return juce::String(value, 2);
            return juce::String(value, 3);
        };
        return std::make_unique<Float>(juce::ParameterID(id, 1), name, std::move(range), defaultValue,
                                       juce::AudioParameterFloatAttributes()
                                           .withLabel(label)
                                           .withStringFromValueFunction(formatValue));
    };

    layout.add(std::make_unique<Int>(juce::ParameterID(wavetable, 1), "Wavetable", 1, 128, 1));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID(oscillatorLink, 1), "Oscillator 2 Link", false));
    layout.add(makeFloat(position, "Wave Position",
                         juce::NormalisableRange<float>(0.0f, 63.0f, 0.01f), 18.0f));
    layout.add(makeFloat(scan, "Wave Envelope",
                         juce::NormalisableRange<float>(-63.0f, 63.0f, 0.01f), 18.0f));
    layout.add(makeFloat(position2, "Wave 2 Position",
                         juce::NormalisableRange<float>(0.0f, 63.0f, 0.01f), 18.0f));
    layout.add(makeFloat(scan2, "Wave 2 Envelope",
                         juce::NormalisableRange<float>(-63.0f, 63.0f, 0.01f), 18.0f));
    for (size_t oscillator = 0; oscillator < oscillatorDetune.size(); ++oscillator)
    {
        const auto number = juce::String(oscillator + 1);
        layout.add(makeFloat(oscillatorDetune[oscillator],
                             "Oscillator " + number + " Detune",
                             juce::NormalisableRange<float>(-64.0f, 63.0f, 0.01f),
                             0.0f, "ct"));
        layout.add(std::make_unique<Int>(
            juce::ParameterID(oscillatorSemitone[oscillator], 1),
            "Oscillator " + number + " Semitone", -12, 12, 0));
        layout.add(std::make_unique<Int>(
            juce::ParameterID(wavePhase[oscillator], 1),
            "Wave " + number + " Start Phase", 0, 127, 0));
        layout.add(makeFloat(waveEnvelopeVelocity[oscillator],
                             "Wave " + number + " Envelope Velocity",
                             juce::NormalisableRange<float>(-64.0f, 63.0f, 0.01f),
                             0.0f));
        layout.add(makeFloat(waveKeytrack[oscillator],
                             "Wave " + number + " Keytrack",
                             juce::NormalisableRange<float>(-64.0f, 63.0f, 0.01f),
                             0.0f));
        layout.add(makeFloat(waveLevel[oscillator],
                             "Wave " + number + " Level",
                             juce::NormalisableRange<float>(0.0f, 1.125f, 0.001f),
                             0.5f));
    }
    for (size_t index = 0; index < waveEnvelopeTime.size(); ++index)
    {
        layout.add(std::make_unique<Int>(juce::ParameterID(waveEnvelopeTime[index], 1),
                                         "Wave Envelope Time " + juce::String(index + 1),
                                         0, 127, 0));
        layout.add(std::make_unique<Int>(juce::ParameterID(waveEnvelopeLevel[index], 1),
                                         "Wave Envelope Level " + juce::String(index + 1),
                                         0, 127, 0));
    }
    layout.add(std::make_unique<Int>(juce::ParameterID(waveEnvelopeKeyOff, 1),
                                     "Wave Envelope Key Off Point", 1, 8, 1));
    layout.add(std::make_unique<Int>(juce::ParameterID(waveEnvelopeLoopStart, 1),
                                     "Wave Envelope Loop Start", 1, 8, 1));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID(waveEnvelopeLoopEnabled, 1), "Wave Envelope Loop", false));
    for (size_t index = 0; index < performanceFader.size(); ++index)
        layout.add(makeFloat(performanceFader[index],
                             "Performance Fader " + juce::String(index + 1),
                             juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
    for (size_t lfo = 0; lfo < lfoRate.size(); ++lfo)
    {
        const auto number = juce::String(lfo + 1);
        layout.add(std::make_unique<Int>(juce::ParameterID(lfoRate[lfo], 1),
                                         "LFO " + number + " Rate", 0, 127, 0));
        layout.add(std::make_unique<Int>(juce::ParameterID(lfoShape[lfo], 1),
                                         "LFO " + number + " Shape", 0, 5, 0));
        layout.add(std::make_unique<Int>(juce::ParameterID(lfoSymmetry[lfo], 1),
                                         "LFO " + number + " Symmetry", -64, 63, 0));
        layout.add(std::make_unique<Int>(juce::ParameterID(lfoHumanize[lfo], 1),
                                         "LFO " + number + " Humanize", 0, 7, 0));
        layout.add(std::make_unique<Int>(juce::ParameterID(lfoSync[lfo], 1),
                                         "LFO " + number + " Sync", 0, 2, 0));
        layout.add(std::make_unique<Int>(juce::ParameterID(lfoPhase[lfo], 1),
                                         "LFO " + number + " Phase", 0, 360, 0));
    }
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID(glideActive, 1), "Glide Active", false));
    layout.add(std::make_unique<Int>(juce::ParameterID(glideType, 1),
                                     "Glide Type", 1, 6, 1));
    layout.add(std::make_unique<Int>(juce::ParameterID(glideRate, 1),
                                     "Glide Rate", 0, 127, 0));
    layout.add(std::make_unique<Int>(juce::ParameterID(glideTimeMode, 1),
                                     "Glide Time/Distance", 0, 1, 0));
    layout.add(std::make_unique<Int>(juce::ParameterID(glideRateModSource, 1),
                                     "Glide Rate Mod Source", 0, 39, 38));
    layout.add(std::make_unique<Int>(juce::ParameterID(glideRateModAmount, 1),
                                     "Glide Rate Mod Amount", -64, 63, 0));
    for (size_t route = 0; route < modulationSource.size(); ++route)
    {
        const auto number = juce::String(route + 1);
        layout.add(std::make_unique<Int>(juce::ParameterID(modulationSource[route], 1),
                                         "Modulation " + number + " Source", 0, 39, 37));
        layout.add(std::make_unique<Int>(juce::ParameterID(modulationControl[route], 1),
                                         "Modulation " + number + " Control", 0, 39, 38));
        layout.add(std::make_unique<Int>(juce::ParameterID(modulationAmount[route], 1),
                                         "Modulation " + number + " Amount", -64, 63, 0));
    }
    layout.add(makeFloat(balance, "Oscillator Balance",
                         juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f));
    layout.add(makeFloat(detune, "Oscillator Detune",
                         juce::NormalisableRange<float>(-50.0f, 50.0f, 0.01f), 7.0f, "ct"));
    layout.add(makeFloat(noise, "Noise",
                         juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

    const auto maximumCutoff = cutoffFrequencyForStep(127.0f);
    auto frequencyRange = juce::NormalisableRange<float>(
        20.0f, maximumCutoff,
        [](float, float, float normalised) {
            return cutoffFrequencyForStep(normalised * 127.0f);
        },
        [](float, float, float frequencyHz) {
            return cutoffStepForFrequency(frequencyHz) / 127.0f;
        },
        [](float, float, float frequencyHz) {
            return cutoffFrequencyForStep(std::round(cutoffStepForFrequency(frequencyHz)));
        });
    layout.add(makeFloat(cutoff, "Cutoff", frequencyRange, 8200.0f, "Hz"));
    layout.add(std::make_unique<Int>(juce::ParameterID(filterMode, 1),
                                     "Filter Mode", 0, 3, 0));
    layout.add(makeFloat(resonance, "Resonance",
                         juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.18f));
    layout.add(makeFloat(filterEnv, "Filter Envelope",
                         juce::NormalisableRange<float>(-64.0f, 63.0f, 0.01f), 28.0f));
    layout.add(makeFloat(filterVelocity, "Filter Velocity",
                         juce::NormalisableRange<float>(-64.0f, 63.0f, 0.01f), 0.0f));
    layout.add(makeFloat(filterKeytrack, "Filter Keytrack",
                         juce::NormalisableRange<float>(-64.0f, 63.0f, 0.01f), 0.0f));
    layout.add(std::make_unique<Int>(juce::ParameterID(filterKeyCenter, 1),
                                     "Filter Key Center", 0, 127, 60));
    layout.add(makeFloat(drive, "Circuit Drive",
                         juce::NormalisableRange<float>(0.0f, 18.0f, 0.01f), 2.0f, "dB"));

    auto timeRange = juce::NormalisableRange<float>(0.001f, 130.0f, 0.0f, 0.2f);
    layout.add(makeFloat(attack, "Attack", timeRange, 0.008f, "s"));
    layout.add(makeFloat(decay, "Decay", timeRange, 0.45f, "s"));
    layout.add(makeFloat(sustain, "Sustain",
                         juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.72f));
    layout.add(makeFloat(release, "Release", timeRange, 0.65f, "s"));
    auto delayRange = juce::NormalisableRange<float>(0.0f, 130.0f, 0.0f, 0.2f);
    layout.add(makeFloat(filterDelay, "Filter Envelope Delay", delayRange, 0.0f, "s"));
    layout.add(makeFloat(filterAttack, "Filter Envelope Attack", timeRange, 0.008f, "s"));
    layout.add(makeFloat(filterDecay, "Filter Envelope Decay", timeRange, 0.45f, "s"));
    layout.add(makeFloat(filterSustain, "Filter Envelope Sustain",
                         juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.72f));
    layout.add(makeFloat(filterRelease, "Filter Envelope Release", timeRange, 0.65f, "s"));
    for (size_t stage = 0; stage < filterEnvelopeModSource.size(); ++stage)
    {
        layout.add(std::make_unique<Int>(
            juce::ParameterID(filterEnvelopeModSource[stage], 1),
            "Filter Envelope Stage " + juce::String(stage + 1) + " Mod Source",
            0, 39, 37));
        layout.add(std::make_unique<Int>(
            juce::ParameterID(filterEnvelopeModAmount[stage], 1),
            "Filter Envelope Stage " + juce::String(stage + 1) + " Mod Amount",
            -64, 63, 0));
    }
    layout.add(makeFloat(highpassCutoff, "High-pass Cutoff", frequencyRange, 20.0f, "Hz"));
    layout.add(std::make_unique<Int>(juce::ParameterID(highpassEnvelopeSelect, 1),
                                     "High-pass Envelope", 0, 3, 1));
    layout.add(makeFloat(highpassEnvelopeAmount, "High-pass Envelope Amount",
                         juce::NormalisableRange<float>(-64.0f, 63.0f, 0.01f), 0.0f));
    layout.add(makeFloat(highpassVelocity, "High-pass Velocity",
                         juce::NormalisableRange<float>(-64.0f, 63.0f, 0.01f), 0.0f));
    layout.add(makeFloat(highpassKeytrack, "High-pass Keytrack",
                         juce::NormalisableRange<float>(-64.0f, 63.0f, 0.01f), 0.0f));
    layout.add(std::make_unique<Int>(juce::ParameterID(highpassKeyCenter, 1),
                                     "High-pass Key Center", 0, 127, 60));
    layout.add(makeFloat(bandpassBandwidth, "Band-pass Bandwidth",
                         juce::NormalisableRange<float>(0.0f, 127.0f, 0.01f), 0.0f));
    for (size_t point = 0; point < freeEnvelopeTime.size(); ++point)
    {
        layout.add(std::make_unique<Int>(juce::ParameterID(freeEnvelopeTime[point], 1),
                                         "Free Envelope Time " + juce::String(point + 1),
                                         0, 127, 0));
        layout.add(std::make_unique<Int>(juce::ParameterID(freeEnvelopeLevel[point], 1),
                                         "Free Envelope Level " + juce::String(point + 1),
                                         -64, 63, 0));
    }
    layout.add(std::make_unique<Int>(juce::ParameterID(freeEnvelopeZeroAxis, 1),
                                     "Free Envelope Zero Axis", -64, 63, 0));
    constexpr std::array<const char*, quickEditCount> quickEditNames {
        "Quick Attack", "Quick Decay", "Quick Sustain", "Quick Release",
        "Quick Pitch Mod", "Quick Timbre", "Quick Timbre Mod", "Quick Wavescan"
    };
    for (size_t index = 0; index < quickEdit.size(); ++index)
        layout.add(makeFloat(quickEdit[index], quickEditNames[index],
                             juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f),
                             0.0f));
    layout.add(makeFloat(pan, "Pan",
                         juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f), 0.0f));
    layout.add(std::make_unique<Int>(juce::ParameterID(panMode, 1),
                                     "Pan Modulation Mode", 0, 2, 1));
    layout.add(makeFloat(output, "Output",
                         juce::NormalisableRange<float>(-30.0f, 6.0f, 0.01f), -7.0f, "dB"));
    layout.add(makeFloat(circuitAge, "Circuit Age",
                         juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.18f));
    return layout;
}

Snapshot readSnapshot(const juce::AudioProcessorValueTreeState& state)
{
    Snapshot result;
    result.wavetableIndex = juce::roundToInt(read(state, wavetable)) - 1;
    result.oscillatorLinkEnabled = read(state, oscillatorLink) >= 0.5f;
    result.wavePosition = read(state, position);
    result.waveScan = read(state, scan);
    result.wavePosition2 = read(state, position2);
    result.waveScan2 = read(state, scan2);
    for (size_t oscillator = 0; oscillator < result.oscillatorDetuneCents.size();
         ++oscillator)
    {
        result.oscillatorDetuneCents[oscillator]
            = read(state, oscillatorDetune[oscillator]);
        result.oscillatorSemitones[oscillator]
            = read(state, oscillatorSemitone[oscillator]);
        result.wavePhases[oscillator] = read(state, wavePhase[oscillator]);
        result.waveEnvelopeVelocityAmounts[oscillator]
            = read(state, waveEnvelopeVelocity[oscillator]);
        result.waveKeytrackAmounts[oscillator]
            = read(state, waveKeytrack[oscillator]);
        result.waveLevels[oscillator] = read(state, waveLevel[oscillator]);
    }
    for (size_t index = 0; index < waveEnvelopeTime.size(); ++index)
    {
        result.waveEnvelopeTimes[index] = read(state, waveEnvelopeTime[index]);
        result.waveEnvelopeLevels[index] = read(state, waveEnvelopeLevel[index]);
    }
    result.waveEnvelopeKeyOffPoint = juce::roundToInt(read(state, waveEnvelopeKeyOff)) - 1;
    result.waveEnvelopeLoopStartPoint = juce::roundToInt(read(state, waveEnvelopeLoopStart)) - 1;
    result.waveEnvelopeLoop = read(state, waveEnvelopeLoopEnabled) >= 0.5f;
    for (size_t lfo = 0; lfo < result.lfos.size(); ++lfo)
    {
        result.lfos[lfo].rate = read(state, lfoRate[lfo]);
        result.lfos[lfo].shape = juce::roundToInt(read(state, lfoShape[lfo]));
        result.lfos[lfo].symmetry = read(state, lfoSymmetry[lfo]);
        result.lfos[lfo].humanize = juce::roundToInt(read(state, lfoHumanize[lfo]));
        result.lfos[lfo].sync = juce::roundToInt(read(state, lfoSync[lfo]));
        result.lfos[lfo].phaseDegrees = read(state, lfoPhase[lfo]);
    }
    result.glideEnabled = read(state, glideActive) >= 0.5f;
    result.glideTypeMode = juce::roundToInt(read(state, glideType));
    result.glideRateValue = read(state, glideRate);
    result.glideTimeModeValue = juce::roundToInt(read(state, glideTimeMode));
    result.glideRateModulationSource
        = juce::roundToInt(read(state, glideRateModSource));
    result.glideRateModulationAmount = read(state, glideRateModAmount);
    for (size_t route = 0; route < result.modulationRoutes.size(); ++route)
    {
        result.modulationRoutes[route].source = juce::roundToInt(
            read(state, modulationSource[route]));
        result.modulationRoutes[route].control = juce::roundToInt(
            read(state, modulationControl[route]));
        result.modulationRoutes[route].amount = read(state, modulationAmount[route]);
    }
    result.oscillatorBalance = read(state, balance);
    result.detuneCents = read(state, detune);
    result.noiseLevel = read(state, noise);
    result.cutoffHz = read(state, cutoff);
    result.filterMode = juce::roundToInt(read(state, filterMode));
    result.resonanceAmount = read(state, resonance);
    result.filterEnvelopeSemitones = read(state, filterEnv);
    result.filterVelocitySemitones = read(state, filterVelocity);
    result.filterKeytrackAmount = read(state, filterKeytrack);
    result.filterKeyCenterNote = juce::roundToInt(read(state, filterKeyCenter));
    result.filterDelaySeconds = read(state, filterDelay);
    result.filterAttackSeconds = read(state, filterAttack);
    result.filterDecaySeconds = read(state, filterDecay);
    result.filterSustainLevel = read(state, filterSustain);
    result.filterReleaseSeconds = read(state, filterRelease);
    for (size_t stage = 0; stage < result.filterEnvelopeModSources.size(); ++stage)
    {
        result.filterEnvelopeModSources[stage]
            = juce::roundToInt(read(state, filterEnvelopeModSource[stage]));
        result.filterEnvelopeModAmounts[stage]
            = read(state, filterEnvelopeModAmount[stage]);
    }
    result.highpassCutoffHz = read(state, highpassCutoff);
    result.highpassEnvelopeSelector = juce::roundToInt(read(state, highpassEnvelopeSelect));
    result.highpassEnvelopeSemitones = read(state, highpassEnvelopeAmount);
    result.highpassVelocitySemitones = read(state, highpassVelocity);
    result.highpassKeytrackAmount = read(state, highpassKeytrack);
    result.highpassKeyCenterNote = juce::roundToInt(read(state, highpassKeyCenter));
    result.bandpassBandwidthSemitones = read(state, bandpassBandwidth);
    for (size_t point = 0; point < result.freeEnvelopeTimes.size(); ++point)
    {
        result.freeEnvelopeTimes[point] = read(state, freeEnvelopeTime[point]);
        result.freeEnvelopeLevels[point] = read(state, freeEnvelopeLevel[point]);
    }
    result.freeEnvelopeZeroAxis = read(state, freeEnvelopeZeroAxis);
    for (size_t index = 0; index < result.quickEditAmounts.size(); ++index)
        result.quickEditAmounts[index] = read(state, quickEdit[index]);
    result.driveDb = read(state, drive);
    result.attackSeconds = read(state, attack);
    result.decaySeconds = read(state, decay);
    result.sustainLevel = read(state, sustain);
    result.releaseSeconds = read(state, release);
    result.panAmount = read(state, pan);
    result.panModulationMode = juce::roundToInt(read(state, panMode));
    result.outputDb = read(state, output);
    result.circuitAgeAmount = read(state, circuitAge);
    return result;
}

void writeSnapshot(juce::AudioProcessorValueTreeState& state, const Snapshot& snapshot,
                   bool includeGlobalParameters)
{
    const auto write = [&state](const char* id, float value) {
        if (auto* parameter = state.getParameter(id))
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    };

    write(wavetable, static_cast<float>(snapshot.wavetableIndex + 1));
    write(oscillatorLink, snapshot.oscillatorLinkEnabled ? 1.0f : 0.0f);
    write(position, snapshot.wavePosition);
    write(scan, snapshot.waveScan);
    write(position2, snapshot.wavePosition2);
    write(scan2, snapshot.waveScan2);
    for (size_t oscillator = 0; oscillator < snapshot.oscillatorDetuneCents.size();
         ++oscillator)
    {
        write(oscillatorDetune[oscillator], snapshot.oscillatorDetuneCents[oscillator]);
        write(oscillatorSemitone[oscillator], snapshot.oscillatorSemitones[oscillator]);
        write(wavePhase[oscillator], snapshot.wavePhases[oscillator]);
        write(waveEnvelopeVelocity[oscillator],
              snapshot.waveEnvelopeVelocityAmounts[oscillator]);
        write(waveKeytrack[oscillator], snapshot.waveKeytrackAmounts[oscillator]);
        write(waveLevel[oscillator], snapshot.waveLevels[oscillator]);
    }
    for (size_t point = 0; point < snapshot.waveEnvelopeTimes.size(); ++point)
    {
        write(waveEnvelopeTime[point], snapshot.waveEnvelopeTimes[point]);
        write(waveEnvelopeLevel[point], snapshot.waveEnvelopeLevels[point]);
    }
    write(waveEnvelopeKeyOff, static_cast<float>(snapshot.waveEnvelopeKeyOffPoint + 1));
    write(waveEnvelopeLoopStart,
          static_cast<float>(snapshot.waveEnvelopeLoopStartPoint + 1));
    write(waveEnvelopeLoopEnabled, snapshot.waveEnvelopeLoop ? 1.0f : 0.0f);
    for (size_t lfo = 0; lfo < snapshot.lfos.size(); ++lfo)
    {
        const auto& source = snapshot.lfos[lfo];
        write(lfoRate[lfo], source.rate);
        write(lfoShape[lfo], static_cast<float>(source.shape));
        write(lfoSymmetry[lfo], source.symmetry);
        write(lfoHumanize[lfo], static_cast<float>(source.humanize));
        write(lfoSync[lfo], static_cast<float>(source.sync));
        write(lfoPhase[lfo], source.phaseDegrees);
    }
    write(glideActive, snapshot.glideEnabled ? 1.0f : 0.0f);
    write(glideType, static_cast<float>(snapshot.glideTypeMode));
    write(glideRate, snapshot.glideRateValue);
    write(glideTimeMode, static_cast<float>(snapshot.glideTimeModeValue));
    write(glideRateModSource,
          static_cast<float>(snapshot.glideRateModulationSource));
    write(glideRateModAmount, snapshot.glideRateModulationAmount);
    for (size_t route = 0; route < snapshot.modulationRoutes.size(); ++route)
    {
        const auto& source = snapshot.modulationRoutes[route];
        write(modulationSource[route], static_cast<float>(source.source));
        write(modulationControl[route], static_cast<float>(source.control));
        write(modulationAmount[route], source.amount);
    }
    write(balance, snapshot.oscillatorBalance);
    write(detune, snapshot.detuneCents);
    write(noise, snapshot.noiseLevel);
    write(cutoff, snapshot.cutoffHz);
    write(filterMode, static_cast<float>(snapshot.filterMode));
    write(resonance, snapshot.resonanceAmount);
    write(filterEnv, snapshot.filterEnvelopeSemitones);
    write(filterVelocity, snapshot.filterVelocitySemitones);
    write(filterKeytrack, snapshot.filterKeytrackAmount);
    write(filterKeyCenter, static_cast<float>(snapshot.filterKeyCenterNote));
    write(filterDelay, snapshot.filterDelaySeconds);
    write(filterAttack, snapshot.filterAttackSeconds);
    write(filterDecay, snapshot.filterDecaySeconds);
    write(filterSustain, snapshot.filterSustainLevel);
    write(filterRelease, snapshot.filterReleaseSeconds);
    for (size_t stage = 0; stage < snapshot.filterEnvelopeModSources.size(); ++stage)
    {
        write(filterEnvelopeModSource[stage],
              static_cast<float>(snapshot.filterEnvelopeModSources[stage]));
        write(filterEnvelopeModAmount[stage], snapshot.filterEnvelopeModAmounts[stage]);
    }
    write(highpassCutoff, snapshot.highpassCutoffHz);
    write(highpassEnvelopeSelect, static_cast<float>(snapshot.highpassEnvelopeSelector));
    write(highpassEnvelopeAmount, snapshot.highpassEnvelopeSemitones);
    write(highpassVelocity, snapshot.highpassVelocitySemitones);
    write(highpassKeytrack, snapshot.highpassKeytrackAmount);
    write(highpassKeyCenter, static_cast<float>(snapshot.highpassKeyCenterNote));
    write(bandpassBandwidth, snapshot.bandpassBandwidthSemitones);
    for (size_t point = 0; point < snapshot.freeEnvelopeTimes.size(); ++point)
    {
        write(freeEnvelopeTime[point], snapshot.freeEnvelopeTimes[point]);
        write(freeEnvelopeLevel[point], snapshot.freeEnvelopeLevels[point]);
    }
    write(freeEnvelopeZeroAxis, snapshot.freeEnvelopeZeroAxis);
    for (size_t index = 0; index < snapshot.quickEditAmounts.size(); ++index)
        write(quickEdit[index], snapshot.quickEditAmounts[index]);
    write(drive, snapshot.driveDb);
    write(attack, snapshot.attackSeconds);
    write(decay, snapshot.decaySeconds);
    write(sustain, snapshot.sustainLevel);
    write(release, snapshot.releaseSeconds);
    write(pan, snapshot.panAmount);
    write(panMode, static_cast<float>(snapshot.panModulationMode));
    if (includeGlobalParameters)
    {
        write(output, snapshot.outputDb);
        write(circuitAge, snapshot.circuitAgeAmount);
    }
}

void applyQuickEdit(Snapshot& snapshot) noexcept
{
    const auto timeScale = [](float amount) {
        return std::exp2(juce::jlimit(-1.0f, 1.0f, amount) * 4.0f);
    };
    const auto scaleTime = [&timeScale](float value, float amount) {
        return juce::jlimit(0.0f, 130.0f, value * timeScale(amount));
    };
    const auto scaleAmount = [](float value, float amount) {
        return juce::jlimit(-64.0f, 63.0f,
                            value * (1.0f + juce::jlimit(-1.0f, 1.0f, amount)));
    };

    const auto attackAmount = snapshot.quickEditAmounts[quickAttack];
    snapshot.attackSeconds = scaleTime(snapshot.attackSeconds, attackAmount);
    snapshot.filterAttackSeconds = scaleTime(snapshot.filterAttackSeconds, attackAmount);
    snapshot.waveEnvelopeTimes[0] = juce::jlimit(
        0.0f, 127.0f, snapshot.waveEnvelopeTimes[0] + attackAmount * 48.0f);
    snapshot.freeEnvelopeTimes[0] = juce::jlimit(
        0.0f, 127.0f, snapshot.freeEnvelopeTimes[0] + attackAmount * 48.0f);

    const auto decayAmount = snapshot.quickEditAmounts[quickDecay];
    snapshot.decaySeconds = scaleTime(snapshot.decaySeconds, decayAmount);
    snapshot.filterDecaySeconds = scaleTime(snapshot.filterDecaySeconds, decayAmount);
    if (snapshot.waveEnvelopeKeyOffPoint >= 1)
        snapshot.waveEnvelopeTimes[1] = juce::jlimit(
            0.0f, 127.0f, snapshot.waveEnvelopeTimes[1] + decayAmount * 48.0f);
    snapshot.freeEnvelopeTimes[1] = juce::jlimit(
        0.0f, 127.0f, snapshot.freeEnvelopeTimes[1] + decayAmount * 48.0f);

    const auto sustainAmount = snapshot.quickEditAmounts[quickSustain];
    snapshot.sustainLevel = juce::jlimit(
        0.0f, 1.0f, snapshot.sustainLevel + sustainAmount * 0.5f);
    snapshot.filterSustainLevel = juce::jlimit(
        0.0f, 1.0f, snapshot.filterSustainLevel + sustainAmount * 0.5f);
    const auto keyOff = static_cast<size_t>(juce::jlimit(
        0, 7, snapshot.waveEnvelopeKeyOffPoint));
    snapshot.waveEnvelopeLevels[keyOff] = juce::jlimit(
        0.0f, 127.0f, snapshot.waveEnvelopeLevels[keyOff] + sustainAmount * 64.0f);
    snapshot.freeEnvelopeLevels[2] = juce::jlimit(
        -64.0f, 63.0f, snapshot.freeEnvelopeLevels[2] + sustainAmount * 64.0f);

    const auto releaseAmount = snapshot.quickEditAmounts[quickRelease];
    snapshot.releaseSeconds = scaleTime(snapshot.releaseSeconds, releaseAmount);
    snapshot.filterReleaseSeconds = scaleTime(snapshot.filterReleaseSeconds, releaseAmount);
    for (size_t point = keyOff + 1; point < snapshot.waveEnvelopeTimes.size(); ++point)
        snapshot.waveEnvelopeTimes[point] = juce::jlimit(
            0.0f, 127.0f, snapshot.waveEnvelopeTimes[point] + releaseAmount * 48.0f);
    snapshot.freeEnvelopeTimes[3] = juce::jlimit(
        0.0f, 127.0f, snapshot.freeEnvelopeTimes[3] + releaseAmount * 48.0f);

    const auto pitchAmount = snapshot.quickEditAmounts[quickPitchMod];
    for (const auto route : { osc1PitchMod1, osc1PitchMod2,
                              osc2PitchMod1, osc2PitchMod2 })
        snapshot.modulationRoutes[static_cast<size_t>(route)].amount = scaleAmount(
            snapshot.modulationRoutes[static_cast<size_t>(route)].amount, pitchAmount);

    const auto timbreAmount = snapshot.quickEditAmounts[quickTimbre];
    snapshot.wavePosition = juce::jlimit(
        0.0f, 63.0f, snapshot.wavePosition + timbreAmount * 20.0f);
    snapshot.wavePosition2 = juce::jlimit(
        0.0f, 63.0f, snapshot.wavePosition2 + timbreAmount * 20.0f);
    const auto cutoffStep = cutoffStepForFrequency(snapshot.cutoffHz) + timbreAmount * 24.0f;
    snapshot.cutoffHz = cutoffFrequencyForStep(cutoffStep);
    snapshot.resonanceAmount = juce::jlimit(
        0.0f, 1.0f, snapshot.resonanceAmount + timbreAmount * 0.35f);

    const auto timbreModAmount = snapshot.quickEditAmounts[quickTimbreMod];
    for (const auto route : { wave1Mod1, wave1Mod2, wave2Mod1, wave2Mod2,
                              filterMod1, filterMod2 })
        snapshot.modulationRoutes[static_cast<size_t>(route)].amount = scaleAmount(
            snapshot.modulationRoutes[static_cast<size_t>(route)].amount,
            timbreModAmount);

    const auto wavescanAmount = snapshot.quickEditAmounts[quickWavescan];
    snapshot.waveScan = scaleAmount(snapshot.waveScan, wavescanAmount);
    snapshot.waveScan2 = scaleAmount(snapshot.waveScan2, wavescanAmount);
}
} // namespace wave::parameters

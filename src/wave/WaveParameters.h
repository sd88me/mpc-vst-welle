#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>

namespace wave::parameters
{
inline constexpr auto wavetable = "wavetable";
inline constexpr auto oscillatorLink = "oscillatorLink";
inline constexpr auto position = "position";
inline constexpr auto scan = "scan";
inline constexpr auto position2 = "position2";
inline constexpr auto scan2 = "scan2";
inline constexpr std::array<const char*, 2> oscillatorDetune {
    "oscillator1Detune", "oscillator2Detune"
};
inline constexpr std::array<const char*, 2> oscillatorSemitone {
    "oscillator1Semitone", "oscillator2Semitone"
};
inline constexpr std::array<const char*, 2> waveEnvelopeVelocity {
    "wave1EnvelopeVelocity", "wave2EnvelopeVelocity"
};
inline constexpr std::array<const char*, 2> wavePhase {
    "wave1Phase", "wave2Phase"
};
inline constexpr std::array<const char*, 2> waveKeytrack {
    "wave1Keytrack", "wave2Keytrack"
};
inline constexpr std::array<const char*, 2> waveLevel {
    "wave1Level", "wave2Level"
};
inline constexpr std::array<const char*, 8> waveEnvelopeTime {
    "waveEnvTime1", "waveEnvTime2", "waveEnvTime3", "waveEnvTime4",
    "waveEnvTime5", "waveEnvTime6", "waveEnvTime7", "waveEnvTime8"
};
inline constexpr std::array<const char*, 8> waveEnvelopeLevel {
    "waveEnvLevel1", "waveEnvLevel2", "waveEnvLevel3", "waveEnvLevel4",
    "waveEnvLevel5", "waveEnvLevel6", "waveEnvLevel7", "waveEnvLevel8"
};
inline constexpr auto waveEnvelopeKeyOff = "waveEnvKeyOff";
inline constexpr auto waveEnvelopeLoopStart = "waveEnvLoopStart";
inline constexpr auto waveEnvelopeLoopEnabled = "waveEnvLoopEnabled";
inline constexpr std::array<const char*, 8> performanceFader {
    "performanceFader1", "performanceFader2", "performanceFader3", "performanceFader4",
    "performanceFader5", "performanceFader6", "performanceFader7", "performanceFader8"
};
inline constexpr std::array<const char*, 2> lfoRate { "lfo1Rate", "lfo2Rate" };
inline constexpr std::array<const char*, 2> lfoShape { "lfo1Shape", "lfo2Shape" };
inline constexpr std::array<const char*, 2> lfoSymmetry { "lfo1Symmetry", "lfo2Symmetry" };
inline constexpr std::array<const char*, 2> lfoHumanize { "lfo1Humanize", "lfo2Humanize" };
inline constexpr std::array<const char*, 2> lfoSync { "lfo1Sync", "lfo2Sync" };
inline constexpr std::array<const char*, 2> lfoPhase { "lfo1Phase", "lfo2Phase" };
inline constexpr auto glideActive = "glideActive";
inline constexpr auto glideType = "glideType";
inline constexpr auto glideRate = "glideRate";
inline constexpr auto glideTimeMode = "glideTimeMode";
inline constexpr auto glideRateModSource = "glideRateModSource";
inline constexpr auto glideRateModAmount = "glideRateModAmount";

enum ModulationRouteIndex
{
    osc1PitchMod1,
    osc1PitchMod2,
    osc2PitchMod1,
    osc2PitchMod2,
    wave1StartMod,
    wave1Mod1,
    wave1Mod2,
    wave2StartMod,
    wave2Mod1,
    wave2Mod2,
    wave1VolumeMod,
    wave2VolumeMod,
    noiseVolumeMod,
    amplifierMod1,
    amplifierMod2,
    filterMod1,
    filterMod2,
    resonanceMod,
    highpassMod1,
    highpassMod2,
    panMod1,
    panMod2,
    lfo1RateMod,
    lfo1LevelMod,
    lfo2RateMod,
    lfo2LevelMod,
    modulationRouteCount
};

inline constexpr std::array<const char*, modulationRouteCount> modulationSource {
    "osc1PitchMod1Source", "osc1PitchMod2Source", "osc2PitchMod1Source",
    "osc2PitchMod2Source", "wave1StartModSource", "wave1Mod1Source",
    "wave1Mod2Source", "wave2StartModSource", "wave2Mod1Source", "wave2Mod2Source",
    "wave1VolumeModSource", "wave2VolumeModSource", "noiseVolumeModSource",
    "amplifierMod1Source", "amplifierMod2Source", "filterMod1Source",
    "filterMod2Source", "resonanceModSource", "highpassMod1Source",
    "highpassMod2Source", "panMod1Source", "panMod2Source",
    "lfo1RateModSource", "lfo1LevelModSource", "lfo2RateModSource", "lfo2LevelModSource"
};
inline constexpr std::array<const char*, modulationRouteCount> modulationControl {
    "osc1PitchMod1Control", "osc1PitchMod2Control", "osc2PitchMod1Control",
    "osc2PitchMod2Control", "wave1StartModControl", "wave1Mod1Control",
    "wave1Mod2Control", "wave2StartModControl", "wave2Mod1Control", "wave2Mod2Control",
    "wave1VolumeModControl", "wave2VolumeModControl", "noiseVolumeModControl",
    "amplifierMod1Control", "amplifierMod2Control", "filterMod1Control",
    "filterMod2Control", "resonanceModControl", "highpassMod1Control",
    "highpassMod2Control", "panMod1Control", "panMod2Control",
    "lfo1RateModControl", "lfo1LevelModControl", "lfo2RateModControl", "lfo2LevelModControl"
};
inline constexpr std::array<const char*, modulationRouteCount> modulationAmount {
    "osc1PitchMod1Amount", "osc1PitchMod2Amount", "osc2PitchMod1Amount",
    "osc2PitchMod2Amount", "wave1StartModAmount", "wave1Mod1Amount",
    "wave1Mod2Amount", "wave2StartModAmount", "wave2Mod1Amount", "wave2Mod2Amount",
    "wave1VolumeModAmount", "wave2VolumeModAmount", "noiseVolumeModAmount",
    "amplifierMod1Amount", "amplifierMod2Amount", "filterMod1Amount",
    "filterMod2Amount", "resonanceModAmount", "highpassMod1Amount",
    "highpassMod2Amount", "panMod1Amount", "panMod2Amount",
    "lfo1RateModAmount", "lfo1LevelModAmount", "lfo2RateModAmount", "lfo2LevelModAmount"
};
inline constexpr auto balance = "balance";
inline constexpr auto detune = "detune";
inline constexpr auto noise = "noise";
inline constexpr auto cutoff = "cutoff";
[[nodiscard]] float cutoffFrequencyForStep(float step) noexcept;
[[nodiscard]] float cutoffStepForFrequency(float frequencyHz) noexcept;
inline constexpr auto filterMode = "filterMode";
inline constexpr auto resonance = "resonance";
inline constexpr auto filterEnv = "filterEnv";
inline constexpr auto filterVelocity = "filterVelocity";
inline constexpr auto filterKeytrack = "filterKeytrack";
inline constexpr auto filterKeyCenter = "filterKeyCenter";
inline constexpr auto filterDelay = "filterDelay";
inline constexpr auto filterAttack = "filterAttack";
inline constexpr auto filterDecay = "filterDecay";
inline constexpr auto filterSustain = "filterSustain";
inline constexpr auto filterRelease = "filterRelease";
inline constexpr std::array<const char*, 4> filterEnvelopeModSource {
    "filterEnvAttackModSource", "filterEnvDecayModSource",
    "filterEnvSustainModSource", "filterEnvReleaseModSource"
};
inline constexpr std::array<const char*, 4> filterEnvelopeModAmount {
    "filterEnvAttackModAmount", "filterEnvDecayModAmount",
    "filterEnvSustainModAmount", "filterEnvReleaseModAmount"
};
inline constexpr auto highpassCutoff = "highpassCutoff";
inline constexpr auto highpassEnvelopeSelect = "highpassEnvelopeSelect";
inline constexpr auto highpassEnvelopeAmount = "highpassEnvelopeAmount";
inline constexpr auto highpassVelocity = "highpassVelocity";
inline constexpr auto highpassKeytrack = "highpassKeytrack";
inline constexpr auto highpassKeyCenter = "highpassKeyCenter";
inline constexpr auto bandpassBandwidth = "bandpassBandwidth";
inline constexpr std::array<const char*, 4> freeEnvelopeTime {
    "freeEnvTime1", "freeEnvTime2", "freeEnvTime3", "freeEnvTime4"
};
inline constexpr std::array<const char*, 4> freeEnvelopeLevel {
    "freeEnvLevel1", "freeEnvLevel2", "freeEnvLevel3", "freeEnvLevel4"
};
inline constexpr auto freeEnvelopeZeroAxis = "freeEnvZeroAxis";
enum QuickEditIndex
{
    quickAttack,
    quickDecay,
    quickSustain,
    quickRelease,
    quickPitchMod,
    quickTimbre,
    quickTimbreMod,
    quickWavescan,
    quickEditCount
};
inline constexpr std::array<const char*, quickEditCount> quickEdit {
    "quickAttack", "quickDecay", "quickSustain", "quickRelease",
    "quickPitchMod", "quickTimbre", "quickTimbreMod", "quickWavescan"
};
inline constexpr auto drive = "drive";
inline constexpr auto attack = "attack";
inline constexpr auto decay = "decay";
inline constexpr auto sustain = "sustain";
inline constexpr auto release = "release";
inline constexpr auto pan = "pan";
inline constexpr auto panMode = "panMode";
inline constexpr auto output = "output";
inline constexpr auto circuitAge = "circuitAge";

struct Snapshot
{
    struct ModulationRoute
    {
        int source = 37;
        int control = 38;
        float amount = 0.0f;
    };

    struct Lfo
    {
        float rate = 0.0f;
        int shape = 0;
        float symmetry = 0.0f;
        int humanize = 0;
        int sync = 0;
        float phaseDegrees = 0.0f;
    };

    int wavetableIndex = 0;
    bool oscillatorLinkEnabled = false;
    std::array<float, 2> oscillatorBendRanges { 2.0f, 2.0f };
    float wavePosition = 18.0f;
    float waveScan = 18.0f;
    float wavePosition2 = 18.0f;
    float waveScan2 = 18.0f;
    std::array<float, 2> oscillatorDetuneCents{};
    std::array<float, 2> oscillatorSemitones{};
    std::array<int, 2> oscillatorOctaves{};
    std::array<float, 2> wavePhases{};
    std::array<float, 2> waveEnvelopeVelocityAmounts{};
    std::array<float, 2> waveKeytrackAmounts{};
    std::array<float, 2> waveLevels { 0.5f, 0.5f };
    std::array<float, 8> waveEnvelopeTimes{};
    std::array<float, 8> waveEnvelopeLevels{};
    int waveEnvelopeKeyOffPoint = 0;
    int waveEnvelopeLoopStartPoint = 0;
    bool waveEnvelopeLoop = false;
    std::array<Lfo, 2> lfos{};
    bool glideEnabled = false;
    int glideTypeMode = 1;
    float glideRateValue = 0.0f;
    int glideTimeModeValue = 0;
    int glideRateModulationSource = 38;
    float glideRateModulationAmount = 0.0f;
    int controlSampleAndHoldSource = 0;
    float controlSampleAndHoldRate = 0.0f;
    int controlSampleAndHoldRateModSource = 37;
    float controlSampleAndHoldRateModAmount = 0.0f;
    int controlComparatorSource = 37;
    float controlComparatorThreshold = 0.0f;
    std::array<ModulationRoute, modulationRouteCount> modulationRoutes{};
    float oscillatorBalance = 0.5f;
    float detuneCents = 7.0f;
    float noiseLevel = 0.0f;
    float cutoffHz = 8200.0f;
    int filterMode = 0;
    float resonanceAmount = 0.18f;
    float filterEnvelopeSemitones = 28.0f;
    float filterVelocitySemitones = 0.0f;
    float filterKeytrackAmount = 0.0f;
    int filterKeyCenterNote = 60;
    float filterDelaySeconds = 0.0f;
    float filterAttackSeconds = 0.008f;
    float filterDecaySeconds = 0.45f;
    float filterSustainLevel = 0.72f;
    float filterReleaseSeconds = 0.65f;
    std::array<int, 4> filterEnvelopeModSources{};
    std::array<float, 4> filterEnvelopeModAmounts{};
    float highpassCutoffHz = 20.0f;
    int highpassEnvelopeSelector = 1;
    float highpassEnvelopeSemitones = 0.0f;
    float highpassVelocitySemitones = 0.0f;
    float highpassKeytrackAmount = 0.0f;
    int highpassKeyCenterNote = 60;
    float bandpassBandwidthSemitones = 0.0f;
    std::array<float, 4> freeEnvelopeTimes{};
    std::array<float, 4> freeEnvelopeLevels{};
    float freeEnvelopeZeroAxis = 0.0f;
    std::array<float, quickEditCount> quickEditAmounts{};
    float driveDb = 2.0f;
    float attackSeconds = 0.008f;
    float decaySeconds = 0.45f;
    float sustainLevel = 0.72f;
    float releaseSeconds = 0.65f;
    float panAmount = 0.0f;
    int panModulationMode = 1;
    float outputDb = -7.0f;
    float circuitAgeAmount = 0.18f;
};

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
Snapshot readSnapshot(const juce::AudioProcessorValueTreeState& state);
void writeSnapshot(juce::AudioProcessorValueTreeState& state, const Snapshot& snapshot,
                   bool includeGlobalParameters = true);
void applyQuickEdit(Snapshot& snapshot) noexcept;
} // namespace wave::parameters

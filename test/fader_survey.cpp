// Survey the performance faders of every performance in the given Wave sets (disk images): what each fader is assigned to,
// whether its target instrument plays, and whether that instrument's sound uses the fader's source anywhere.
// usage: wave_fader_survey <fw dir> <disk.img>...
#include "PluginProcessor.h"
#include <cstdio>
#include <map>
#include <memory>
#include <string>
int main(int argc, char** argv)
{
    auto p = std::make_unique<WaveEmulationAudioProcessor>(juce::File{}, WaveEmulationAudioProcessor::InitialBank::initialised);
    p->prepareToPlay(44100, 128);
    p->loadFirmware(juce::File(argv[1]), false);
    juce::AudioBuffer<float> a(2, 128);
    auto run = [&](int n) { for (int b = 0; b < n; ++b) { juce::MidiBuffer m; a.clear(); p->processBlock(a, m); p->serviceAsyncUpdate(); } };
    run(100);
    std::map<std::string, int> kinds;
    long faders = 0, live = 0, used = 0, perfsWithUsed = 0, perfs = 0, perfsAllSame = 0;
    std::map<int, int> usedPerPerf;
    for (int d = 2; d < argc; ++d) {
        p->mountDiskImage(juce::File(argv[d]));
        p->reloadBankFromMountedImage();
        run(300);
        long setUsed = 0, setPerfsUsed = 0;
        for (int prog = 0; prog < 256; ++prog) {
            p->setCurrentProgram(prog);
            run(260);
            const auto& rt = p->getMasterFirmwareRuntime();
            auto byte = [&](int o) { return (int) (rt.sharedProgramByte(0x5400u + (uint32_t) o) & 0x7f); };
            const int cx = byte(28), cy = byte(29);
            const auto perf = p->getCurrentPerformanceSnapshot();
            int nUsed = 0;
            std::string first; bool same = true;
            for (int f = 0; f < 8; ++f) {
                const int dest = byte(8 + f * 2), as = byte(9 + f * 2);
                char k[48];
                if (dest >= 8) snprintf(k, sizeof k, "external MIDI");
                else if (as <= 116) snprintf(k, sizeof k, "CC %d%s", as + 1, as + 1 == 1 ? " (mod wheel)" : as + 1 == cx ? " (= Control X)" : as + 1 == cy ? " (= Control Y)" : "");
                else snprintf(k, sizeof k, "%s", as == 117 ? "Bend +" : as == 118 ? "Bend -" : as == 119 ? "Bend +/-" : as == 120 ? "Pressure" : as == 121 ? "Control X" : as == 122 ? "Control Y" : "other");
                std::string key = std::string(k) + (dest < 8 ? "" : "");
                if (f == 0) first = k; else if (first != k) same = false;
                kinds[k]++;
                faders++;
                if (dest >= 8 || !perf.layers[(size_t) dest].enabled) continue;
                live++;
                // the sources this fader drives (WaldorfEngine sourceValue numbering)
                int srcs[3] = {-1, -1, -1};
                if (as <= 116) { const int cc = as + 1; if (cc == 1) srcs[0] = 22; if (cc == cx) srcs[1] = 34; if (cc == cy) srcs[2] = 35; }
                else if (as <= 119) srcs[0] = 21; else if (as == 120) srcs[0] = 16; else if (as == 121) srcs[0] = 34; else if (as == 122) srcs[0] = 35;
                const auto& s = perf.layers[(size_t) dest].sound;
                auto uses = [&](int src) {
                    if (src < 0) return false;
                    for (auto& r : s.modulationRoutes) if (r.amount != 0.0f && (r.source == src || r.control == src)) return true;
                    for (int i = 0; i < 4; ++i) if (s.filterEnvelopeModSources[(size_t) i] == src && s.filterEnvelopeModAmounts[(size_t) i] != 0.0f) return true;
                    if (s.glideRateModulationSource == src && s.glideRateModulationAmount != 0.0f) return true;
                    return false;
                };
                if (uses(srcs[0]) || uses(srcs[1]) || uses(srcs[2])) { used++; nUsed++; }
            }
            perfs++; if (same) perfsAllSame++;
            usedPerPerf[nUsed]++;
            if (nUsed) { perfsWithUsed++; setPerfsUsed++; }
            setUsed += nUsed;
        }
        printf("%-40s performances with a working fader: %3ld / 256, working faders %4ld / 2048\n",
               juce::File(argv[d]).getFileNameWithoutExtension().toRawUTF8(), setPerfsUsed, setUsed);
    }
    printf("\nall: %ld performances, %ld faders; %ld point at a playing instrument; %ld change its sound (%.1f%%)\n",
           perfs, faders, live, used, 100.0 * used / faders);
    printf("performances with all 8 faders assigned alike: %ld; with at least one working fader: %ld\n", perfsAllSame, perfsWithUsed);
    printf("working faders per performance:"); for (auto& [n, c] : usedPerPerf) printf("  %d: %d", n, c); printf("\n\nassignments:\n");
    for (auto& [k, n] : kinds) printf("  %6d  %s\n", n, k.c_str());
}

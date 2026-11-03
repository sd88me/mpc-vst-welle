#include "PluginProcessor.h"
#include <cstdio>
#include <memory>
int main(int argc, char** argv) {
    auto p = std::make_unique<WaveEmulationAudioProcessor>(juce::File{}, WaveEmulationAudioProcessor::InitialBank::initialised);
    p->prepareToPlay(44100, 128); p->loadFirmware(juce::File(argv[1]), false);
    juce::AudioBuffer<float> a(2, 128); auto run = [&](int n) { for (int b = 0; b < n; ++b) { juce::MidiBuffer m; a.clear(); p->processBlock(a, m); p->serviceAsyncUpdate(); } };
    run(100); p->mountDiskImage(juce::File(argv[2])); p->reloadBankFromMountedImage(); run(300);
    for (int prog : {3, 0, 13}) {
        p->setCurrentProgram(prog); run(700);
        auto s = p->getCurrentPerformanceSnapshot();
        printf("%s\n", p->getProgramName(prog).toRawUTF8());
        for (int l = 0; l < 8; ++l) { auto& y = s.layers[(size_t) l]; if (!y.enabled) continue;
            printf("  layer %d enabled out=%d gain=%.2f aux=%.2f ch=%d keys %d-%d\n", l, y.audioOutput, y.gain, y.auxGain, y.midiChannel, y.keyLow, y.keyHigh); }
    }
}

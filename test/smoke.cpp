// Boot the firmware headless, play a chord, report level and speed.
#include "PluginProcessor.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
int main(int argc, char** argv)
{
    juce::MessageManager::getInstance(); std::puts("mm ok");
    auto t0 = std::chrono::steady_clock::now();
    auto pp = std::make_unique<WaveEmulationAudioProcessor>(juce::File{}, WaveEmulationAudioProcessor::InitialBank::initialised); auto& p = *pp;
    const double sr = 44100; const int bs = 128;
    p.prepareToPlay(sr, bs);
    if (argc > 3) p.setVoiceLimit(atoi(argv[3]));
    if (getenv("NOTHREAD")) p.setVoiceCardThreading(false);
    if (argc > 1) {
        auto r = p.loadFirmware(juce::File(argv[1]), false);
        std::printf("firmware loaded=%d\n", p.getMasterFirmwareRuntime().isLoaded());
    }
    std::printf("boot %.2fs programs=%d\n", std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count(), p.getNumPrograms());
    for (int i : {0, 1, 2, 100, 128, 255}) std::printf("prog %d: %s\n", i, p.getProgramName(i).toRawUTF8());
    { juce::MemoryBlock st; p.getStateInformation(st); std::printf("state %d bytes\n", (int) st.getSize()); }
    juce::AudioBuffer<float> a(2, bs);
    int voices = argc > 2 ? atoi(argv[2]) : 4; double secs = 5;
    int blocks = int(secs * sr / bs); float peak = 0;
    auto t1 = std::chrono::steady_clock::now();
    for (int b = 0; b < blocks; ++b) {
        juce::MidiBuffer m;
        if (b == int(0.5 * sr / bs)) for (int v = 0; v < voices; ++v) m.addEvent(juce::MidiMessage::noteOn(1, 48 + v * 4, 0.8f), 0);
        if (b == int(4 * sr / bs)) for (int v = 0; v < voices; ++v) m.addEvent(juce::MidiMessage::noteOff(1, 48 + v * 4), 0);
        a.clear(); p.processBlock(a, m); if (getenv("SERVICE")) p.serviceAsyncUpdate();
        peak = std::max(peak, a.getMagnitude(0, bs));
    }
    double w = std::chrono::duration<double>(std::chrono::steady_clock::now() - t1).count();
    std::printf("peak %.3f active=%d  %.1f%% of realtime (wall)\n", peak, p.getActiveVoiceCount(), 100 * w / secs);
}

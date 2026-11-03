// gprof driver: a factory performance, 8 notes held, voice cards rendered serially (one thread). usage: wave_prof <fw dir> <disk.img> [prog]
#include "PluginProcessor.h"
#include <cstdio>
#include <memory>
#include <chrono>
#include <cstdlib>
#include "sampler.h"
int main(int argc, char** argv)
{
    auto p = std::make_unique<WaveEmulationAudioProcessor>(juce::File{}, WaveEmulationAudioProcessor::InitialBank::initialised);
    p->prepareToPlay(44100, 128);
    p->setVoiceLimit(8);
    p->setVoiceCardThreading(false);
    p->loadFirmware(juce::File(argv[1]), false);
    juce::AudioBuffer<float> a(2, 128);
    auto run = [&](int blocks, juce::MidiBuffer m) { for (int b = 0; b < blocks; ++b) { a.clear(); juce::MidiBuffer mm; if (b == 0) mm = m; p->processBlock(a, mm); p->serviceAsyncUpdate(); } };
    run(100, {});
    p->mountDiskImage(juce::File(argv[2]));
    p->reloadBankFromMountedImage();
    run(300, {});
    p->setCurrentProgram(argc > 3 ? atoi(argv[3]) : 0);
    run(700, {});
    juce::MidiBuffer m;
    for (int v = 0; v < 8; ++v) m.addEvent(juce::MidiMessage::noteOn(1, 48 + 3 * v, 0.8f), 0);
    std::printf("%s, playing\n", p->getProgramName(argc > 3 ? atoi(argv[3]) : 0).toRawUTF8());
    const auto t0 = std::chrono::steady_clock::now();
    if (getenv("SAMPLE")) sampler::start();
    run(44100 * 20 / 128, m);
    if (getenv("SAMPLE")) sampler::stop(getenv("SAMPLE"));
    std::printf("20 s of audio in %.2f s (%.0f%% of a core)\n", std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count(),
                5.0 * std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
    std::printf("active %d\n", p->getActiveVoiceCount());
}

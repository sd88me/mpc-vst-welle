// Boot, run a few seconds, write the LCD as a PGM (480 x 64) for font/layout study.
#include "PluginProcessor.h"
#include "UI/WaveLcdModel.h"
#include <cstdio>
#include <memory>
int main(int argc, char** argv)
{
    auto p = std::make_unique<WaveEmulationAudioProcessor>(juce::File{}, WaveEmulationAudioProcessor::InitialBank::initialised);
    p->prepareToPlay(44100, 128);
    p->loadFirmware(juce::File(argv[1]), false);
    juce::AudioBuffer<float> a(2, 128);
    for (int b = 0; b < 44100 * 4 / 128; ++b) { juce::MidiBuffer m; a.clear(); p->processBlock(a, m); p->serviceAsyncUpdate(); }
    const auto& rt = p->getMasterFirmwareRuntime();
    auto v = rt.lcdVideoSnapshot();
    wave::ui::LcdFramebuffer fb;
    fb.loadHardwareVideoRam(v.data(), v.size(), rt.lcdDisplayPage());
    FILE* f = fopen(argv[2], "wb");
    fprintf(f, "P5 480 64 255\n");
    for (int y = 0; y < 64; ++y) for (int x = 0; x < 480; ++x) fputc(fb.pixel(x, y) ? 0 : 255, f);
    fclose(f);
}

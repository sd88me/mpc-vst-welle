// Stands in for the header juce_add_binary_data generates upstream (WaveAssets / WavePpgAssets).
#pragma once
extern "C" const char wave_blank_img[], wave_blank_img_end[], wave_ppg_bin[], wave_ppg_bin_end[];
namespace WaveAssets {
inline const char* const BlankWave_img = wave_blank_img;
inline const int BlankWave_imgSize = int(wave_blank_img_end - wave_blank_img);
}
namespace WavePpgAssets {
inline const char* const ppgv6wavetables_bin = wave_ppg_bin;
inline const int ppgv6wavetables_binSize = int(wave_ppg_bin_end - wave_ppg_bin);
}

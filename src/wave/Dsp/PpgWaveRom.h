#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace wave::dsp
{
class PpgWaveRom
{
public:
    static constexpr size_t tableDirectoryBytes = 768;
    static constexpr size_t waveformCount = 256;
    static constexpr size_t halfWaveSamples = 64;
    static constexpr size_t waveformBytes = waveformCount * halfWaveSamples;
    static constexpr size_t minimumImageBytes = tableDirectoryBytes + waveformBytes;
    static constexpr int storedTableCount = 28;
    static constexpr int decodedTableCount = 30;
    static constexpr int wavesPerTable = 64;
    static constexpr int samplesPerWave = 128;

    struct Result
    {
        bool success = false;
        int decodedTables = 0;
        std::string detail;
    };

    // Decodes a user-supplied PPG Wave 2.2/2.3 V6 wavetable EPROM image. The
    // image contains a 768-byte sparse table directory followed by 256
    // unsigned eight-bit, 64-sample half waves. The public sample bank contains decoded waveforms, not an executable
    // ROM image. Tables 28/29 and the shared analogue tail
    // waves are generated with the arithmetic used by the V6 6809 firmware.
    static Result decode(const void* image, size_t imageSize,
                         std::span<int8_t> destination);
};
} // namespace wave::dsp

#include "PpgWaveRom.h"

#include <algorithm>
#include <array>
#include <vector>

namespace wave::dsp
{
namespace
{
struct KeyWave
{
    uint8_t waveform = 0;
    uint8_t position = 0;
};

using TableKeys = std::vector<KeyWave>;

constexpr size_t outputSamples = static_cast<size_t>(PpgWaveRom::decodedTableCount)
                                 * PpgWaveRom::wavesPerTable
                                 * PpgWaveRom::samplesPerWave;

size_t outputIndex(int table, int wave, int sample) noexcept
{
    return (static_cast<size_t>(table) * PpgWaveRom::wavesPerTable
            + static_cast<size_t>(wave))
           * PpgWaveRom::samplesPerWave + static_cast<size_t>(sample);
}

int8_t signedSample(int value) noexcept
{
    return static_cast<int8_t>(std::clamp(value, -128, 127));
}

void writeUnsigned(std::span<int8_t> output, int table, int wave, int sample,
                   uint8_t value) noexcept
{
    output[outputIndex(table, wave, sample)]
        = signedSample(static_cast<int>(value) - 128);
}

void writeClassicTail(std::span<int8_t> output, int table) noexcept
{
    uint8_t value = 0x20;
    for (int sample = 0; sample < 64; ++sample)
    {
        writeUnsigned(output, table, 60, sample, value);
        value = static_cast<uint8_t>(value + 3u);
    }
    for (int sample = 64; sample < 128; ++sample)
    {
        writeUnsigned(output, table, 60, sample, value);
        value = static_cast<uint8_t>(value - 3u);
    }

    for (int sample = 0; sample < 126; ++sample)
        writeUnsigned(output, table, 61, sample, 0x50);
    writeUnsigned(output, table, 61, 126, 0xff);
    writeUnsigned(output, table, 61, 127, 0xff);

    for (int sample = 0; sample < 64; ++sample)
        writeUnsigned(output, table, 62, sample, 0x20);
    for (int sample = 64; sample < 128; ++sample)
        writeUnsigned(output, table, 62, sample, 0xe0);

    value = 0x40;
    for (int sample = 0; sample < 128; ++sample)
    {
        writeUnsigned(output, table, 63, sample, value);
        ++value;
    }
}

void generateFirmwareTable28(std::span<int8_t> output) noexcept
{
    uint16_t step = 0x00ea;
    for (int wave = 0; wave < 60; ++wave)
    {
        step = static_cast<uint16_t>(step + 0x0016u);
        uint16_t phase = 0x8000;
        for (int sample = 0; sample < 128; ++sample)
        {
            const auto high = static_cast<uint8_t>(phase >> 8u);
            writeUnsigned(output, 28, wave, sample,
                          static_cast<uint8_t>(high + 0xc0u));
            const auto sum = static_cast<uint32_t>(phase) + step;
            phase = sum > 0xffffu ? 0x8000 : static_cast<uint16_t>(sum);
        }
    }
    writeClassicTail(output, 28);
}

void generateFirmwareTable29(std::span<int8_t> output) noexcept
{
    for (int wave = 0; wave < 60; ++wave)
    {
        const auto threshold = 60 - wave;
        auto value = static_cast<uint8_t>(0xa0);
        for (int sample = 0; sample < 128; ++sample)
        {
            writeUnsigned(output, 29, wave, sample, value);
            const auto counter = 127 - sample;
            if (counter == threshold)
                value = 0x60;
        }
    }
    writeClassicTail(output, 29);
}
} // namespace

PpgWaveRom::Result PpgWaveRom::decode(const void* image, size_t imageSize,
                                      std::span<int8_t> destination)
{
    Result result;
    if (image == nullptr || imageSize < minimumImageBytes || destination.size() < outputSamples)
    {
        result.detail = "PPG image is shorter than its table directory and waveform bank";
        return result;
    }

    const auto* bytes = static_cast<const uint8_t*>(image);
    std::array<TableKeys, storedTableCount + 1> tables; // Includes the stored Upper Wavetable.
    size_t cursor = 0;

    for (auto& keys : tables)
    {
        if (cursor >= tableDirectoryBytes)
        {
            result.detail = "PPG table directory ended before all stored tables were decoded";
            return result;
        }

        ++cursor; // Table number/marker; known images do not use it consistently.
        int previousPosition = -1;
        for (;;)
        {
            if (cursor + 1 >= tableDirectoryBytes)
            {
                result.detail = "PPG sparse table entry crosses the 768-byte directory boundary";
                return result;
            }

            const auto waveform = bytes[cursor++];
            const auto position = bytes[cursor++];
            if (position > 63 || static_cast<int>(position) <= previousPosition)
            {
                result.detail = "PPG sparse table positions are invalid or not increasing";
                return result;
            }
            keys.push_back({ waveform, position });
            previousPosition = position;
            if (position >= 60)
                break;
        }

        if (keys.empty() || keys.front().position != 0 || keys.back().position != 60)
        {
            result.detail = "PPG sparse table is missing its slot 0 or slot 60 boundary key";
            return result;
        }
    }

    std::vector<int8_t> decoded(outputSamples, 0);
    const auto* halfWaves = bytes + tableDirectoryBytes;

    for (int table = 0; table < storedTableCount; ++table)
    {
        const auto& keys = tables[static_cast<size_t>(table)];
        std::array<std::array<uint8_t, halfWaveSamples>, 61> expanded{};
        std::array<bool, 61> populated{};
        for (const auto& key : keys)
        {
            if (key.position > 60)
            {
                result.detail = "PPG key wave lies beyond the firmware interpolation workspace";
                return result;
            }
            auto& destinationWave = expanded[key.position];
            const auto* sourceWave = halfWaves
                                     + static_cast<size_t>(key.waveform) * halfWaveSamples;
            std::copy_n(sourceWave, halfWaveSamples, destinationWave.begin());
            populated[key.position] = true;
        }

        // The V6 firmware repeatedly fills the midpoint of every remaining
        // gap. Its ADDA/RORA sequence is an unsigned floor average, which is
        // observably different from direct linear interpolation.
        for (;;)
        {
            auto complete = true;
            auto left = 0;
            while (left < 60)
            {
                while (left <= 60 && !populated[static_cast<size_t>(left)])
                    ++left;
                if (left >= 60)
                    break;
                auto right = left + 1;
                while (right <= 60 && !populated[static_cast<size_t>(right)])
                    ++right;
                if (right > 60)
                {
                    result.detail = "PPG sparse table does not bracket every lower-wave slot";
                    return result;
                }
                if (right - left > 1)
                {
                    const auto midpoint = left + (right - left) / 2;
                    const auto leftIndex = static_cast<size_t>(left);
                    const auto rightIndex = static_cast<size_t>(right);
                    const auto midpointIndex = static_cast<size_t>(midpoint);
                    for (size_t sample = 0; sample < halfWaveSamples; ++sample)
                    {
                        const auto sum = static_cast<unsigned>(expanded[leftIndex][sample])
                                         + static_cast<unsigned>(expanded[rightIndex][sample]);
                        expanded[midpointIndex][sample] = static_cast<uint8_t>(sum >> 1u);
                    }
                    populated[midpointIndex] = true;
                    complete = false;
                }
                left = right;
            }
            if (complete)
                break;
        }

        for (int wave = 0; wave < 60; ++wave)
        {
            for (int sample = 0; sample < 64; ++sample)
            {
                const auto value = expanded[static_cast<size_t>(wave)]
                                           [static_cast<size_t>(sample)];
                writeUnsigned(decoded, table, wave, sample, value);
                writeUnsigned(decoded, table, wave, 127 - sample,
                              static_cast<uint8_t>(~value));
            }
        }
        writeClassicTail(decoded, table);
    }

    generateFirmwareTable28(decoded);
    generateFirmwareTable29(decoded);

    std::copy(decoded.begin(), decoded.end(), destination.begin());
    result.success = true;
    result.decodedTables = decodedTableCount;
    result.detail = "Decoded 28 stored tables and generated PPG V6 tables 28/29";
    return result;
}
} // namespace wave::dsp

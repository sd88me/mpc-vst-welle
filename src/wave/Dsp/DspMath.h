#pragma once

#include <array>
#include <cstddef>

namespace wave::dsp::math
{
extern const std::array<float, 128> midiFrequencies;

[[nodiscard]] inline float midiFrequency(int note) noexcept
{
    return midiFrequencies[static_cast<size_t>(note < 0 ? 0 : (note > 127 ? 127 : note))];
}
} // namespace wave::dsp::math

#include "DspMath.h"

#include <cmath>

namespace wave::dsp::math
{
const std::array<float, 128> midiFrequencies = [] {
    std::array<float, 128> result{};
    for (size_t note = 0; note < result.size(); ++note)
        result[note] = 440.0f * std::exp2((static_cast<float>(note) - 69.0f) / 12.0f);
    return result;
}();
} // namespace wave::dsp::math

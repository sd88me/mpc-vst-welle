#include "WaveLcdModel.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace wave::ui
{
void LcdFramebuffer::clear(bool illuminated) noexcept
{
    memory.fill(illuminated ? 0xffu : 0x00u);
}

void LcdFramebuffer::loadHardwareVideoRam(const uint8_t* videoRam, size_t videoRamSize,
                                          uint8_t displayPage) noexcept
{
    clear();
    if (videoRam == nullptr)
        return;

    constexpr size_t pageSize = 0x1000;
    constexpr size_t hardwareBytesPerRow = 64;
    constexpr size_t scanlines = static_cast<size_t>(height);
    const auto pageOffset = static_cast<size_t>(displayPage & 0x03u) * pageSize;
    const auto requiredBytes = pageOffset + hardwareBytesPerRow * scanlines;
    if (videoRamSize < requiredBytes)
        return;

    // The genuine OS character renderer advances 64 bytes between pixel rows.
    // The first 60 bytes are the visible 480 pixels and the remaining four are
    // horizontal-blanking padding in each 4 KiB hardware display page.
    for (size_t row = 0; row < scanlines; ++row)
        std::copy_n(videoRam + pageOffset + row * hardwareBytesPerRow,
                    static_cast<size_t>(bytesPerRow),
                    memory.begin() + row * static_cast<size_t>(bytesPerRow));
}

void LcdFramebuffer::write(uint16_t address, uint8_t value) noexcept
{
    memory[static_cast<size_t>(address) % memory.size()] = value;
}

uint8_t LcdFramebuffer::read(uint16_t address) const noexcept
{
    return memory[static_cast<size_t>(address) % memory.size()];
}

void LcdFramebuffer::setPixel(int x, int y, bool illuminated) noexcept
{
    if (x < 0 || x >= width || y < 0 || y >= height)
        return;

    const auto byteIndex = static_cast<size_t>(y * bytesPerRow + x / 8);
    const auto mask = static_cast<uint8_t>(0x80u >> static_cast<unsigned>(x & 7));
    if (illuminated)
        memory[byteIndex] |= mask;
    else
        memory[byteIndex] &= static_cast<uint8_t>(~mask);
}

bool LcdFramebuffer::pixel(int x, int y) const noexcept
{
    if (x < 0 || x >= width || y < 0 || y >= height)
        return false;

    const auto byteIndex = static_cast<size_t>(y * bytesPerRow + x / 8);
    const auto mask = static_cast<uint8_t>(0x80u >> static_cast<unsigned>(x & 7));
    return (memory[byteIndex] & mask) != 0;
}

void LcdFramebuffer::horizontalLine(int x1, int x2, int y, bool illuminated) noexcept
{
    if (x1 > x2)
        std::swap(x1, x2);
    for (auto x = x1; x <= x2; ++x)
        setPixel(x, y, illuminated);
}

void LcdFramebuffer::verticalLine(int x, int y1, int y2, bool illuminated) noexcept
{
    if (y1 > y2)
        std::swap(y1, y2);
    for (auto y = y1; y <= y2; ++y)
        setPixel(x, y, illuminated);
}

void LcdFramebuffer::line(int x1, int y1, int x2, int y2, bool illuminated) noexcept
{
    const auto dx = std::abs(x2 - x1);
    const auto sx = x1 < x2 ? 1 : -1;
    const auto dy = -std::abs(y2 - y1);
    const auto sy = y1 < y2 ? 1 : -1;
    auto error = dx + dy;

    for (;;)
    {
        setPixel(x1, y1, illuminated);
        if (x1 == x2 && y1 == y2)
            break;
        const auto twiceError = error * 2;
        if (twiceError >= dy)
        {
            error += dy;
            x1 += sx;
        }
        if (twiceError <= dx)
        {
            error += dx;
            y1 += sy;
        }
    }
}

void LcdFramebuffer::rectangle(int x, int y, int rectangleWidth, int rectangleHeight,
                               bool illuminated) noexcept
{
    if (rectangleWidth <= 0 || rectangleHeight <= 0)
        return;
    horizontalLine(x, x + rectangleWidth - 1, y, illuminated);
    horizontalLine(x, x + rectangleWidth - 1, y + rectangleHeight - 1, illuminated);
    verticalLine(x, y, y + rectangleHeight - 1, illuminated);
    verticalLine(x + rectangleWidth - 1, y, y + rectangleHeight - 1, illuminated);
}

void LcdFramebuffer::fillRectangle(int x, int y, int rectangleWidth, int rectangleHeight,
                                   bool illuminated) noexcept
{
    for (auto row = 0; row < rectangleHeight; ++row)
        horizontalLine(x, x + rectangleWidth - 1, y + row, illuminated);
}

void LcdFramebuffer::text(int x, int y, std::string_view value, int scale,
                          bool illuminated) noexcept
{
    scale = std::max(1, scale);
    auto cursor = x;
    for (const auto character : value)
    {
        const auto columns = glyph(character);
        for (int column = 0; column < 5; ++column)
        {
            for (int row = 0; row < 7; ++row)
            {
                if ((columns[static_cast<size_t>(column)] & (1u << static_cast<unsigned>(row))) == 0)
                    continue;
                fillRectangle(cursor + column * scale, y + row * scale, scale, scale, illuminated);
            }
        }
        cursor += 6 * scale;
    }
}

std::array<uint8_t, 5> LcdFramebuffer::glyph(char character) noexcept
{
    const auto c = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
    switch (c)
    {
        case ' ': return { 0x00, 0x00, 0x00, 0x00, 0x00 };
        case '!': return { 0x00, 0x00, 0x5f, 0x00, 0x00 };
        case '#': return { 0x14, 0x7f, 0x14, 0x7f, 0x14 };
        case '%': return { 0x23, 0x13, 0x08, 0x64, 0x62 };
        case '+': return { 0x08, 0x08, 0x3e, 0x08, 0x08 };
        case ',': return { 0x00, 0x50, 0x30, 0x00, 0x00 };
        case '-': return { 0x08, 0x08, 0x08, 0x08, 0x08 };
        case '.': return { 0x00, 0x60, 0x60, 0x00, 0x00 };
        case '/': return { 0x20, 0x10, 0x08, 0x04, 0x02 };
        case '0': return { 0x3e, 0x51, 0x49, 0x45, 0x3e };
        case '1': return { 0x00, 0x42, 0x7f, 0x40, 0x00 };
        case '2': return { 0x42, 0x61, 0x51, 0x49, 0x46 };
        case '3': return { 0x21, 0x41, 0x45, 0x4b, 0x31 };
        case '4': return { 0x18, 0x14, 0x12, 0x7f, 0x10 };
        case '5': return { 0x27, 0x45, 0x45, 0x45, 0x39 };
        case '6': return { 0x3c, 0x4a, 0x49, 0x49, 0x30 };
        case '7': return { 0x01, 0x71, 0x09, 0x05, 0x03 };
        case '8': return { 0x36, 0x49, 0x49, 0x49, 0x36 };
        case '9': return { 0x06, 0x49, 0x49, 0x29, 0x1e };
        case ':': return { 0x00, 0x36, 0x36, 0x00, 0x00 };
        case '<': return { 0x08, 0x14, 0x22, 0x41, 0x00 };
        case '=': return { 0x14, 0x14, 0x14, 0x14, 0x14 };
        case '>': return { 0x00, 0x41, 0x22, 0x14, 0x08 };
        case '?': return { 0x02, 0x01, 0x51, 0x09, 0x06 };
        case 'A': return { 0x7e, 0x11, 0x11, 0x11, 0x7e };
        case 'B': return { 0x7f, 0x49, 0x49, 0x49, 0x36 };
        case 'C': return { 0x3e, 0x41, 0x41, 0x41, 0x22 };
        case 'D': return { 0x7f, 0x41, 0x41, 0x22, 0x1c };
        case 'E': return { 0x7f, 0x49, 0x49, 0x49, 0x41 };
        case 'F': return { 0x7f, 0x09, 0x09, 0x09, 0x01 };
        case 'G': return { 0x3e, 0x41, 0x49, 0x49, 0x7a };
        case 'H': return { 0x7f, 0x08, 0x08, 0x08, 0x7f };
        case 'I': return { 0x00, 0x41, 0x7f, 0x41, 0x00 };
        case 'J': return { 0x20, 0x40, 0x41, 0x3f, 0x01 };
        case 'K': return { 0x7f, 0x08, 0x14, 0x22, 0x41 };
        case 'L': return { 0x7f, 0x40, 0x40, 0x40, 0x40 };
        case 'M': return { 0x7f, 0x02, 0x0c, 0x02, 0x7f };
        case 'N': return { 0x7f, 0x04, 0x08, 0x10, 0x7f };
        case 'O': return { 0x3e, 0x41, 0x41, 0x41, 0x3e };
        case 'P': return { 0x7f, 0x09, 0x09, 0x09, 0x06 };
        case 'Q': return { 0x3e, 0x41, 0x51, 0x21, 0x5e };
        case 'R': return { 0x7f, 0x09, 0x19, 0x29, 0x46 };
        case 'S': return { 0x46, 0x49, 0x49, 0x49, 0x31 };
        case 'T': return { 0x01, 0x01, 0x7f, 0x01, 0x01 };
        case 'U': return { 0x3f, 0x40, 0x40, 0x40, 0x3f };
        case 'V': return { 0x1f, 0x20, 0x40, 0x20, 0x1f };
        case 'W': return { 0x3f, 0x40, 0x38, 0x40, 0x3f };
        case 'X': return { 0x63, 0x14, 0x08, 0x14, 0x63 };
        case 'Y': return { 0x07, 0x08, 0x70, 0x08, 0x07 };
        case 'Z': return { 0x61, 0x51, 0x49, 0x45, 0x43 };
        case '[': return { 0x00, 0x7f, 0x41, 0x41, 0x00 };
        case ']': return { 0x00, 0x41, 0x41, 0x7f, 0x00 };
        case '_': return { 0x40, 0x40, 0x40, 0x40, 0x40 };
        default: return { 0x02, 0x01, 0x51, 0x09, 0x06 };
    }
}
} // namespace wave::ui

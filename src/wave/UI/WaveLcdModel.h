#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace wave::ui
{
class LcdFramebuffer
{
public:
    static constexpr int width = 480;
    static constexpr int height = 64;
    static constexpr int bytesPerRow = width / 8;
    static constexpr int memorySize = bytesPerRow * height;

    void clear(bool illuminated = false) noexcept;
    void loadHardwareVideoRam(const uint8_t* videoRam, size_t videoRamSize,
                              uint8_t displayPage) noexcept;
    void write(uint16_t address, uint8_t value) noexcept;
    [[nodiscard]] uint8_t read(uint16_t address) const noexcept;

    void setPixel(int x, int y, bool illuminated = true) noexcept;
    [[nodiscard]] bool pixel(int x, int y) const noexcept;
    void horizontalLine(int x1, int x2, int y, bool illuminated = true) noexcept;
    void verticalLine(int x, int y1, int y2, bool illuminated = true) noexcept;
    void line(int x1, int y1, int x2, int y2, bool illuminated = true) noexcept;
    void rectangle(int x, int y, int rectangleWidth, int rectangleHeight,
                   bool illuminated = true) noexcept;
    void fillRectangle(int x, int y, int rectangleWidth, int rectangleHeight,
                       bool illuminated = true) noexcept;
    void text(int x, int y, std::string_view value, int scale = 1,
              bool illuminated = true) noexcept;

    [[nodiscard]] const std::array<uint8_t, memorySize>& data() const noexcept { return memory; }

private:
    [[nodiscard]] static std::array<uint8_t, 5> glyph(char character) noexcept;
    std::array<uint8_t, memorySize> memory{};
};
} // namespace wave::ui

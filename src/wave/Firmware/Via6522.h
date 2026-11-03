#pragma once

#include <array>
#include <cstdint>
#include <limits>

namespace wave::firmware
{
// MOS 6522 connected to the main CPU board's E clock. The Wave maps the VIA's
// sixteen registers onto odd byte addresses at $BE0001-$BE001F.
class Via6522
{
public:
    void reset() noexcept;

    [[nodiscard]] uint8_t read(uint8_t reg, uint8_t portAInputs = 0,
                               uint8_t portBInputs = 0) noexcept;
    void write(uint8_t reg, uint8_t value) noexcept;

    [[nodiscard]] int advanceCpuCycles(int cpuCycles) noexcept;
    [[nodiscard]] uint32_t cpuCyclesUntilTimerEvent() const noexcept;
    [[nodiscard]] bool interruptAsserted() const noexcept;

private:
    void clearInterruptFlag(uint8_t mask) noexcept;
    [[nodiscard]] uint32_t timerPeriodCpuCycles() const noexcept;

    std::array<uint8_t, 16> registers{};
    uint16_t timer1Latch = 0xffffu;
    uint32_t timer1CpuCyclesRemaining = 0;
    uint8_t interruptFlags = 0;
    uint8_t interruptEnable = 0;
    bool timer1Running = false;

    // The generated E-clock period is ten emulated master-CPU cycles.
    static constexpr uint32_t cpuCyclesPerViaClock = 10;
    static constexpr uint8_t timer1Interrupt = 0x40u;
};
} // namespace wave::firmware

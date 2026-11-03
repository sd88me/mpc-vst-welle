#include "Via6522.h"

#include <algorithm>

namespace wave::firmware
{
namespace
{
constexpr uint8_t outputB = 0;
constexpr uint8_t outputA = 1;
constexpr uint8_t dataDirectionB = 2;
constexpr uint8_t dataDirectionA = 3;
constexpr uint8_t timer1CounterLow = 4;
constexpr uint8_t timer1CounterHigh = 5;
constexpr uint8_t timer1LatchLow = 6;
constexpr uint8_t timer1LatchHigh = 7;
constexpr uint8_t auxiliaryControl = 11;
constexpr uint8_t interruptFlagRegister = 13;
constexpr uint8_t interruptEnableRegister = 14;
constexpr uint8_t outputANoHandshake = 15;
}

void Via6522::reset() noexcept
{
    registers.fill(0);
    timer1Latch = 0xffffu;
    timer1CpuCyclesRemaining = 0;
    interruptFlags = 0;
    interruptEnable = 0;
    timer1Running = false;
}

uint8_t Via6522::read(uint8_t reg, uint8_t portAInputs,
                      uint8_t portBInputs) noexcept
{
    reg &= 0x0fu;
    if (reg == outputA || reg == outputANoHandshake)
        return static_cast<uint8_t>((registers[outputA] & registers[dataDirectionA])
                                    | (portAInputs & ~registers[dataDirectionA]));
    if (reg == outputB)
        return static_cast<uint8_t>((registers[outputB] & registers[dataDirectionB])
                                    | (portBInputs & ~registers[dataDirectionB]));
    if (reg == timer1CounterLow || reg == timer1CounterHigh)
    {
        const auto viaClocks = timer1Running
                                   ? (timer1CpuCyclesRemaining
                                      + cpuCyclesPerViaClock - 1u)
                                         / cpuCyclesPerViaClock
                                   : 0u;
        if (reg == timer1CounterLow)
            clearInterruptFlag(timer1Interrupt);
        return reg == timer1CounterLow ? static_cast<uint8_t>(viaClocks)
                                       : static_cast<uint8_t>(viaClocks >> 8u);
    }
    if (reg == interruptFlagRegister)
        return static_cast<uint8_t>(interruptFlags
                                    | (interruptAsserted() ? 0x80u : 0u));
    if (reg == interruptEnableRegister)
        return static_cast<uint8_t>(interruptEnable | 0x80u);
    return registers[reg];
}

void Via6522::write(uint8_t reg, uint8_t value) noexcept
{
    reg &= 0x0fu;
    if (reg == outputANoHandshake)
        reg = outputA;

    switch (reg)
    {
        case timer1CounterLow:
            timer1Latch = static_cast<uint16_t>((timer1Latch & 0xff00u) | value);
            registers[timer1LatchLow] = value;
            break;
        case timer1CounterHigh:
            timer1Latch = static_cast<uint16_t>((timer1Latch & 0x00ffu)
                                                | static_cast<uint16_t>(value << 8u));
            registers[timer1LatchHigh] = value;
            timer1CpuCyclesRemaining = timerPeriodCpuCycles();
            timer1Running = true;
            clearInterruptFlag(timer1Interrupt);
            break;
        case timer1LatchLow:
            timer1Latch = static_cast<uint16_t>((timer1Latch & 0xff00u) | value);
            break;
        case timer1LatchHigh:
            timer1Latch = static_cast<uint16_t>((timer1Latch & 0x00ffu)
                                                | static_cast<uint16_t>(value << 8u));
            clearInterruptFlag(timer1Interrupt);
            break;
        case interruptFlagRegister:
            clearInterruptFlag(value);
            return;
        case interruptEnableRegister:
            if ((value & 0x80u) != 0u)
                interruptEnable = static_cast<uint8_t>(interruptEnable | (value & 0x7fu));
            else
                interruptEnable = static_cast<uint8_t>(interruptEnable & ~(value & 0x7fu));
            registers[interruptEnableRegister] = interruptEnable;
            return;
        default:
            break;
    }
    registers[reg] = value;
}

int Via6522::advanceCpuCycles(int cpuCycles) noexcept
{
    if (!timer1Running || cpuCycles <= 0)
        return 0;

    auto remaining = static_cast<uint32_t>(cpuCycles);
    auto expirations = 0;
    while (timer1Running && remaining >= timer1CpuCyclesRemaining)
    {
        remaining -= timer1CpuCyclesRemaining;
        interruptFlags = static_cast<uint8_t>(interruptFlags | timer1Interrupt);
        ++expirations;
        if ((registers[auxiliaryControl] & 0x40u) != 0u)
            timer1CpuCyclesRemaining = timerPeriodCpuCycles();
        else
            timer1Running = false;
    }
    if (timer1Running)
        timer1CpuCyclesRemaining -= remaining;
    return expirations;
}

uint32_t Via6522::cpuCyclesUntilTimerEvent() const noexcept
{
    return timer1Running ? std::max(1u, timer1CpuCyclesRemaining)
                         : std::numeric_limits<uint32_t>::max();
}

bool Via6522::interruptAsserted() const noexcept
{
    return (interruptFlags & interruptEnable & 0x7fu) != 0u;
}

void Via6522::clearInterruptFlag(uint8_t mask) noexcept
{
    interruptFlags = static_cast<uint8_t>(interruptFlags & ~(mask & 0x7fu));
}

uint32_t Via6522::timerPeriodCpuCycles() const noexcept
{
    return (static_cast<uint32_t>(timer1Latch) + 2u) * cpuCyclesPerViaClock;
}
} // namespace wave::firmware

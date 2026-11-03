#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace wave::firmware
{
class M68000;

class M68000Bus
{
public:
    virtual ~M68000Bus() = default;

    [[nodiscard]] virtual uint8_t read8(uint32_t address) noexcept = 0;
    virtual void write8(uint32_t address, uint8_t value) noexcept = 0;
    [[nodiscard]] virtual bool usesInstructionInterception() const noexcept { return false; }
    virtual bool interceptInstruction(M68000&, uint32_t) noexcept { return false; }
};

class M68000
{
public:
    M68000() = default;

    void reset(M68000Bus& bus);
    void start(M68000Bus& bus, uint32_t stackPointer, uint32_t programCounter);
    int execute(M68000Bus& bus, int cycles);
    void endTimeslice() noexcept;
    void setInterruptLevel(M68000Bus& bus, int level);
    void setDataRegister(M68000Bus& bus, int index, uint32_t value);

    [[nodiscard]] bool isInitialised() const noexcept { return initialised; }
    [[nodiscard]] uint32_t programCounter() const noexcept;
    [[nodiscard]] uint32_t stackPointer() const noexcept;
    [[nodiscard]] uint32_t dataRegister(int index) const noexcept;
    [[nodiscard]] uint32_t addressRegister(int index) const noexcept;
    [[nodiscard]] uint16_t statusRegister() const noexcept;
    [[nodiscard]] int currentExecutionCycleOffset() const noexcept;
    [[nodiscard]] uint32_t fastForwardDbraLoop(M68000Bus& bus, int dataRegister,
                                               uint32_t maximumCycles) noexcept;

    bool returnFromSubroutine(uint32_t dataRegister0) noexcept;

private:
    [[nodiscard]] uint32_t registerValue(int registerId) const noexcept;
    void activate(M68000Bus& bus) const;
    void save();

    std::vector<std::byte> context;
    bool initialised = false;
};
} // namespace wave::firmware

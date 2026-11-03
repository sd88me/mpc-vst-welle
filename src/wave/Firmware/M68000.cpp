#include "M68000.h"

extern "C"
{
#include <m68k.h>
}

#include <algorithm>
#include <mutex>

namespace
{
thread_local wave::firmware::M68000Bus* activeBus = nullptr;
thread_local wave::firmware::M68000* activeCpu = nullptr;
std::once_flag initialiseCore;

// Only callbacks during a core operation may read live registers. A host can
// move a processor to another thread between render calls.
struct ActiveCoreScope
{
    ~ActiveCoreScope()
    {
        activeCpu = nullptr;
        activeBus = nullptr;
    }
};

[[nodiscard]] uint32_t masked(uint32_t address) noexcept
{
    return address & 0x00ffffffu;
}

[[nodiscard]] uint8_t readByte(uint32_t address) noexcept
{
    return activeBus != nullptr ? activeBus->read8(masked(address)) : 0xffu;
}

void writeByte(uint32_t address, uint8_t value) noexcept
{
    if (activeBus != nullptr)
        activeBus->write8(masked(address), value);
}

void instructionHook(unsigned int programCounter) noexcept
{
    if (activeBus != nullptr && activeCpu != nullptr)
        activeBus->interceptInstruction(*activeCpu, masked(programCounter));
}

} // namespace

extern "C" unsigned int m68k_read_memory_8(unsigned int address)
{
    return readByte(address);
}

extern "C" unsigned int m68k_read_memory_16(unsigned int address)
{
    return static_cast<unsigned int>((static_cast<uint16_t>(readByte(address)) << 8u)
                                     | readByte(address + 1u));
}

extern "C" unsigned int m68k_read_memory_32(unsigned int address)
{
    return (m68k_read_memory_16(address) << 16u) | m68k_read_memory_16(address + 2u);
}

extern "C" void m68k_write_memory_8(unsigned int address, unsigned int value)
{
    writeByte(address, static_cast<uint8_t>(value));
}

extern "C" void m68k_write_memory_16(unsigned int address, unsigned int value)
{
    writeByte(address, static_cast<uint8_t>(value >> 8u));
    writeByte(address + 1u, static_cast<uint8_t>(value));
}

extern "C" void m68k_write_memory_32(unsigned int address, unsigned int value)
{
    m68k_write_memory_16(address, value >> 16u);
    m68k_write_memory_16(address + 2u, value);
}

namespace wave::firmware
{
void M68000::reset(M68000Bus& bus)
{
    std::call_once(initialiseCore, [] { m68k_init(); });
    // The opcode tables are shared and initialized once. Default callbacks
    // belong to each thread's core and must also be initialized on that thread.
    m68k_init();
    const ActiveCoreScope scope;
    activeBus = &bus;
    activeCpu = this;
    m68k_set_cpu_type(M68K_CPU_TYPE_68000);
    m68k_pulse_reset();
    context.resize(m68k_context_size());
    save();
    initialised = true;
}

void M68000::start(M68000Bus& bus, uint32_t stackPointerValue, uint32_t programCounterValue)
{
    reset(bus);
    const ActiveCoreScope scope;
    activate(bus);
    m68k_set_reg(M68K_REG_SP, stackPointerValue);
    m68k_set_reg(M68K_REG_PC, programCounterValue);
    save();
}

int M68000::execute(M68000Bus& bus, int cycles)
{
    if (!initialised || cycles <= 0)
        return 0;

    const ActiveCoreScope scope;
    activate(bus);
    const auto executed = m68k_execute(cycles);
    save();
    return executed;
}

void M68000::endTimeslice() noexcept
{
    if (activeCpu == this)
        m68k_end_timeslice();
}

void M68000::setInterruptLevel(M68000Bus& bus, int level)
{
    if (!initialised)
        return;
    const ActiveCoreScope scope;
    activate(bus);
    m68k_set_irq(static_cast<unsigned int>(std::clamp(level, 0, 7)));
    save();
}

void M68000::setDataRegister(M68000Bus& bus, int index, uint32_t value)
{
    if (!initialised || index < 0 || index >= 8)
        return;
    const ActiveCoreScope scope;
    activate(bus);
    m68k_set_reg(static_cast<m68k_register_t>(M68K_REG_D0 + index), value);
    save();
}

uint32_t M68000::programCounter() const noexcept
{
    return registerValue(M68K_REG_PC);
}

uint32_t M68000::stackPointer() const noexcept
{
    return registerValue(M68K_REG_SP);
}

uint32_t M68000::dataRegister(int index) const noexcept
{
    return index >= 0 && index < 8 ? registerValue(M68K_REG_D0 + index) : 0u;
}

uint32_t M68000::addressRegister(int index) const noexcept
{
    return index >= 0 && index < 8 ? registerValue(M68K_REG_A0 + index) : 0u;
}

uint16_t M68000::statusRegister() const noexcept
{
    return static_cast<uint16_t>(registerValue(M68K_REG_SR));
}

bool M68000::returnFromSubroutine(uint32_t dataRegister0) noexcept
{
    if (activeCpu != this || activeBus == nullptr)
        return false;

    const auto stack = m68k_get_reg(nullptr, M68K_REG_SP);
    const auto returnAddress = (static_cast<uint32_t>(activeBus->read8(stack)) << 24u)
                               | (static_cast<uint32_t>(activeBus->read8(stack + 1u)) << 16u)
                               | (static_cast<uint32_t>(activeBus->read8(stack + 2u)) << 8u)
                               | static_cast<uint32_t>(activeBus->read8(stack + 3u));
    m68k_set_reg(M68K_REG_D0, dataRegister0);
    m68k_set_reg(M68K_REG_SP, stack + 4u);
    m68k_set_reg(M68K_REG_PC, returnAddress);
    return true;
}

int M68000::currentExecutionCycleOffset() const noexcept
{
    return activeCpu == this ? m68k_cycles_run() : 0;
}

uint32_t M68000::fastForwardDbraLoop(M68000Bus& bus, int dataRegister,
                                    uint32_t maximumCycles) noexcept
{
    if (!initialised || dataRegister < 0 || dataRegister >= 8)
        return 0;

    const ActiveCoreScope scope;
    activate(bus);
    const auto reg = static_cast<m68k_register_t>(M68K_REG_D0 + dataRegister);
    const auto value = m68k_get_reg(nullptr, reg);
    const auto remainingIterations = value & 0xffffu;
    const auto skippedIterations = std::min(remainingIterations, maximumCycles / 10u);
    const auto skippedCycles = skippedIterations * 10u;
    if (remainingIterations == 0xffffu || skippedCycles == 0u)
    {
        save();
        return 0;
    }

    // Leave the final fall-through DBRA for the core to execute. Each removed
    // taken iteration costs 10 cycles on the MC68000.
    const auto remaining = remainingIterations - skippedIterations;
    m68k_set_reg(reg, (value & 0xffff0000u) | remaining);
    save();
    return skippedCycles;
}

uint32_t M68000::registerValue(int registerId) const noexcept
{
    if (activeCpu == this)
        return m68k_get_reg(nullptr, static_cast<m68k_register_t>(registerId));
    if (!initialised || context.empty())
        return 0u;
    return m68k_get_reg(const_cast<std::byte*>(context.data()),
                        static_cast<m68k_register_t>(registerId));
}

void M68000::activate(M68000Bus& bus) const
{
    activeBus = &bus;
    activeCpu = const_cast<M68000*>(this);
    m68k_set_context(const_cast<std::byte*>(context.data()));
    m68k_set_instr_hook_callback(bus.usesInstructionInterception() ? instructionHook : nullptr);
}

void M68000::save()
{
    m68k_get_context(context.data());
}
} // namespace wave::firmware

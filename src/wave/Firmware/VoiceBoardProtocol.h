#pragma once

#include "SharedFirmwareMemory.h"

#include <cstdint>
#include <span>
#include <vector>

namespace wave::firmware
{
// Decoded external contract used by the genuine WDV service loop. Field
// meanings inside each 256-byte voice record remain named only when supported
// by firmware traces; the transport itself is no longer a synthetic mailbox.
class VoiceBoardProtocol
{
public:
    struct PendingVoice
    {
        int board = 0;
        int voice = 0;
        uint32_t recordOffset = 0;
        uint32_t semaphoreOffset = 0;
    };

    explicit VoiceBoardProtocol(SharedFirmwareMemory& memory) noexcept : shared(memory) {}

    [[nodiscard]] std::vector<PendingVoice> pendingUpdates(int boardCount = 1) const;
    [[nodiscard]] std::span<uint8_t, 256> voiceRecord(int board, int voice) noexcept;
    bool requestUpdate(int board, int voice) noexcept;

    static constexpr uint32_t updateMaskBase = 0x5092;
    static constexpr uint32_t semaphoreBase = 0x50a2;
    static constexpr uint32_t busMutex = 0x50e2;
    static constexpr uint32_t voiceRecordBase = 0x0000;
    static constexpr uint32_t voiceRecordSize = 0x0100;
    static constexpr int voicesPerBoard = 16;
    static constexpr int maximumBoards = 3;

private:
    SharedFirmwareMemory& shared;
};
} // namespace wave::firmware

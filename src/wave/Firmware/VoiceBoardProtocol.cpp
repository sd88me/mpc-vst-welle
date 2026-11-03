#include "VoiceBoardProtocol.h"

#include <algorithm>

namespace wave::firmware
{
std::vector<VoiceBoardProtocol::PendingVoice> VoiceBoardProtocol::pendingUpdates(
    int boardCount) const
{
    std::vector<PendingVoice> result;
    boardCount = std::clamp(boardCount, 0, maximumBoards);
    for (int board = 0; board < boardCount; ++board)
    {
        const auto offset = updateMaskBase + static_cast<uint32_t>(board * 2);
        const auto mask = static_cast<uint16_t>(
            (static_cast<uint16_t>(shared.program[offset]) << 8u)
            | shared.program[offset + 1u]);
        for (int voice = 0; voice < voicesPerBoard; ++voice)
            if ((mask & static_cast<uint16_t>(1u << static_cast<unsigned int>(voice))) != 0u)
                result.push_back({ board, voice,
                                   static_cast<uint32_t>(board * voicesPerBoard + voice)
                                       * voiceRecordSize,
                                   semaphoreBase
                                       + static_cast<uint32_t>(board * voicesPerBoard + voice) });
    }
    return result;
}

std::span<uint8_t, 256> VoiceBoardProtocol::voiceRecord(int board, int voice) noexcept
{
    board = std::clamp(board, 0, maximumBoards - 1);
    voice = std::clamp(voice, 0, voicesPerBoard - 1);
    const auto offset = static_cast<size_t>(board * voicesPerBoard + voice) * voiceRecordSize;
    return std::span<uint8_t, 256>(shared.program.data() + offset, voiceRecordSize);
}

bool VoiceBoardProtocol::requestUpdate(int board, int voice) noexcept
{
    if (board < 0 || board >= maximumBoards || voice < 0 || voice >= voicesPerBoard
        || shared.program[busMutex] != 0x00)
        return false;

    shared.program[busMutex] = 0x80; // 68000 TAS lock value used by wdv.sys.
    const auto offset = updateMaskBase + static_cast<uint32_t>(board * 2);
    auto mask = static_cast<uint16_t>((static_cast<uint16_t>(shared.program[offset]) << 8u)
                                      | shared.program[offset + 1u]);
    mask |= static_cast<uint16_t>(1u << static_cast<unsigned int>(voice));
    shared.program[offset] = static_cast<uint8_t>(mask >> 8u);
    shared.program[offset + 1u] = static_cast<uint8_t>(mask);
    shared.program[busMutex] = 0x00;
    return true;
}
} // namespace wave::firmware

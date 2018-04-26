#pragma once
#include <cstdint>

namespace cafe::coreinit
{

void
OSLogPrintf(uint32_t unk1,
            uint32_t unk2,
            uint32_t unk3,
            const char *fmt,
            ...);

void
OSPanic(const char *file,
        uint32_t line,
        const char *fmt,
        ...);

} // namespace cafe::coreinit

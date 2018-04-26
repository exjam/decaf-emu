#include "coreinit.h"
#include "coreinit_log.h"

namespace cafe::coreinit
{

void
OSLogPrintf(uint32_t unk1,
            uint32_t unk2,
            uint32_t unk3,
            const char *fmt,
            ...)
{
   // TODO: OSLogPrintf
}

void
OSPanic(const char *file,
        uint32_t line,
        const char *fmt,
        ...)
{
   // TODO: OSPanic
}

} // namespace cafe::coreinit

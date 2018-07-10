#include "cafe_kernel_process.h"
#include <cstdint>

namespace cafe::kernel::internal
{

void
KiRPLStartup(UniqueProcessId callerProcessId,
             UniqueProcessId targetProcessId,
             ProcessFlags processFlags,
             uint32_t numCodeAreaHeapBlocks,
             uint32_t maxCodeSize,
             uint32_t maxDataSize,
             uint32_t titleLoc);

} // namespace cafe::kernel::internal

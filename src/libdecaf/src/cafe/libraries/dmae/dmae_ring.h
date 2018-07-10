#pragma once
#include <cstdint>

namespace cafe::dmae
{

enum class DMAEEndianSwapMode : uint32_t
{
   None = 0,
   Swap8In16 = 1,
   Swap8In32 = 2
};

using DMAETimestamp = int64_t;

void
DMAEInit();

DMAETimestamp
DMAEGetLastSubmittedTimeStamp();

DMAETimestamp
DMAEGetRetiredTimeStamp();

uint32_t
DMAEGetTimeout();

void
DMAESetTimeout(uint32_t timeout);

uint64_t
DMAECopyMem(virt_ptr<void> dst,
            virt_ptr<void> src,
            uint32_t numWords,
            DMAEEndianSwapMode endian);

uint64_t
DMAEFillMem(virt_ptr<void> dst,
            uint32_t value,
            uint32_t numDwords);

BOOL
DMAEWaitDone(DMAETimestamp timestamp);

} // namespace cafe::dmae

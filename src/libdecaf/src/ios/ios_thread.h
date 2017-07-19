#pragma once
#include "ios_enum.h"

#include <cstdint>

namespace ios
{

using ThreadID = uint32_t;
using ThreadPriority = int32_t;
using ThreadEntryFn = uint32_t(*)(void *);

struct Thread
{
   ThreadID id;
   IOSProcessID pid;
   ThreadState state;
   ThreadPriority minPriority;
   ThreadPriority maxPriority;
};

} // namespace ios

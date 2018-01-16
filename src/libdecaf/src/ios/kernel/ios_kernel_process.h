#pragma once
#include "ios/ios_enum.h"
#include <cstdint>

namespace ios::kernel
{

Error
IOS_GetCurrentProcessId();

Error
IOS_GetProcessName(ProcessId process,
                   char *buffer);

namespace internal
{

ProcessId
getCurrentProcessId();

} // namespace internal

} // namespace ios::kernel

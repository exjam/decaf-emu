#pragma once
#include "ios_enum.h"
#include "ios_result.h"
#include <cstdint>

namespace ios
{

struct Process
{

};

Result<IOSError>
getProcessName(IOSProcessID process,
               char *buffer);

} // namespace ios

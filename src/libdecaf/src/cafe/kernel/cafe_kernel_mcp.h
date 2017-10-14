#pragma once
#include "ios/ios_enum.h"

namespace cafe::kernel::internal
{

using MCPHandle = int32_t;

ios::Error
MCP_Open();

ios::Error
MCP_Close(MCPHandle handle);

} // namespace cafe::kernel::internal

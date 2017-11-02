#pragma once
#include "ios/ios_ipc.h"

namespace cafe::kernel
{

using MCPHandle = ios::Handle;

MCPHandle
loaderGetMcpHandle();

namespace internal
{

ios::Error
MCP_Open();

ios::Error
MCP_Close(MCPHandle handle);

} // namespace internal

} // namespace cafe::kernel

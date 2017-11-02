#include "cafe_kernel_mcp.h"

namespace cafe::kernel
{

MCPHandle
loaderGetMcpHandle()
{
   return MCPHandle { -1 };
}

namespace internal
{

ios::Error
MCP_Open()
{
   return ios::Error::Invalid;
}

ios::Error
MCP_Close(MCPHandle handle)
{
   return ios::Error::Invalid;
}

} // namespace internal

} // namespace cafe::kernel

#pragma once
#include "cafe/kernel/cafe_kernel_process.h"
#include "ios/ios_enum.h"
#include "ios/mcp/ios_mcp_mcp.h"

#include <libcpu/be2_struct.h>

namespace cafe::loader::internal
{

void
LiInitBuffer(bool unk);

int32_t
LiBounceOneChunk(std::string_view name,
                 ios::mcp::MCPFileType fileType,
                 cafe::kernel::UniqueProcessId upid,
                 virt_ptr<uint32_t> outHunkBytes,
                 uint32_t offset,
                 int32_t bufferNumber,
                 virt_ptr<virt_ptr<void>> outDstPtr);

int32_t
LiWaitOneChunk(uint32_t *outBytesRead,
               std::string_view name,
               ios::mcp::MCPFileType fileType);

} // namespace cafe::loader::internal

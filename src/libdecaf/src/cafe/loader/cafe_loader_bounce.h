#pragma once
#include "cafe_loader_loaded_rpl.h"
#include "cafe/kernel/cafe_kernel_process.h"
#include "ios/ios_enum.h"
#include "ios/mcp/ios_mcp_enum.h"
#include "filesystem/filesystem.h"

#include <libcpu/be2_struct.h>

namespace cafe::loader::internal
{

void
LiInitBuffer(bool unk);

ios::Error
LiBounceOneChunk(std::string_view name,
                 ios::mcp::MCPFileType fileType,
                 kernel::UniqueProcessId upid,
                 uint32_t *outChunkBytes,
                 uint32_t offset,
                 uint32_t bufferNumber,
                 virt_ptr<void> *outChunk);

ios::Error
LiWaitOneChunk(uint32_t *outBytesRead,
               std::string_view name,
               ios::mcp::MCPFileType fileType);

int32_t
LiCleanUpBufferAfterModuleLoaded();

int32_t
LiRefillUpcomingBounceBuffer(virt_ptr<LOADED_RPL> rpl,
                             int32_t bufferNumber);

void
LiCloseBufferIfError();

} // namespace cafe::loader::internal

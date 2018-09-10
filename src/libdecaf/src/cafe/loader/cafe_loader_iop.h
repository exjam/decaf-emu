#pragma once
#include "cafe/kernel/cafe_kernel_processid.h"
#include "ios/mcp/ios_mcp_enum.h"

namespace cafe::loader::internal
{

void
LiInitIopInterface();

ios::Error
LiLoadAsync(std::string_view name,
            virt_ptr<void> outBuffer,
            uint32_t outBufferSize,
            uint32_t pos,
            ios::mcp::MCPFileType fileType,
            cafe::kernel::RamPartitionId rampid);

ios::Error
LiWaitIopComplete(uint32_t *outBytesRead);

ios::Error
LiWaitIopCompleteWithInterrupts(uint32_t *outBytesRead);

void
LiCheckAndHandleInterrupts();

void
initialiseStaticIopData();

} // namespace cafe::loader::internal

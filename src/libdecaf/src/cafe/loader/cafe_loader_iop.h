#pragma once
#include "cafe/kernel/cafe_kernel_process.h"
#include "ios/ios_enum.h"
#include "ios/mcp/ios_mcp_mcp.h"

#include <libcpu/be2_struct.h>

namespace cafe::loader::internal
{

void
LiInitIopInterface();

ios::Error
LiLoadAsync(std::string_view name,
            virt_ptr<void> outBuffer,
            uint32_t outBufferSize,
            uint32_t pos,
            ios::mcp::MCPLibraryType libraryType,
            cafe::kernel::RamProcessId rampid);

ios::Error
LiWaitIopComplete(uint32_t *outBytesRead);

ios::Error
LiWaitIopCompleteWithInterrupts(uint32_t *outBytesRead);

void
LiCheckAndHandleInterrupts();

void
initialiseStaticIopData();

} // namespace cafe::loader::internal

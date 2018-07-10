#pragma once
#include "cafe_loader_loaded_rpl.h"
#include "cafe/cafe_tinyheap.h"
#include "cafe/kernel/cafe_kernel_process.h"
#include "ios/mcp/ios_mcp_enum.h"
#include <libcpu/be2_struct.h>

namespace cafe::loader::internal
{

struct BasicLoadArgs
{
   be2_val<cafe::kernel::UniqueProcessId> upid;
   be2_virt_ptr<LOADED_RPL> loadedRpl;
   be2_virt_ptr<TinyHeap> readHeapTracking;
   be2_val<uint32_t> pathNameLen;
   be2_virt_ptr<char> pathName;
   UNKNOWN(0x4);
   be2_val<ios::mcp::MCPFileType> fileType;
   be2_virt_ptr<void> chunkBuffer;
   be2_val<uint32_t> chunkBufferSize;
   be2_val<uint32_t> fileOffset;
};
CHECK_OFFSET(BasicLoadArgs, 0x00, upid);
CHECK_OFFSET(BasicLoadArgs, 0x04, loadedRpl);
CHECK_OFFSET(BasicLoadArgs, 0x08, readHeapTracking);
CHECK_OFFSET(BasicLoadArgs, 0x0C, pathNameLen);
CHECK_OFFSET(BasicLoadArgs, 0x10, pathName);
CHECK_OFFSET(BasicLoadArgs, 0x18, fileType);
CHECK_OFFSET(BasicLoadArgs, 0x1C, chunkBuffer);
CHECK_OFFSET(BasicLoadArgs, 0x20, chunkBufferSize);
CHECK_OFFSET(BasicLoadArgs, 0x24, fileOffset);
CHECK_SIZE(BasicLoadArgs, 0x28);

int32_t
LiLoadRPLBasics(virt_ptr<char> moduleName,
                uint32_t moduleNameLen,
                virt_ptr<void> chunkBuffer,
                virt_ptr<TinyHeap> codeHeapTracking,
                virt_ptr<TinyHeap> dataHeapTracking,
                bool allocModuleName,
                uint32_t r9,
                virt_ptr<LOADED_RPL> *outLoadedRpl,
                BasicLoadArgs *loadArgs,
                uint32_t arg_C);

} // namespace cafe::loader::internal

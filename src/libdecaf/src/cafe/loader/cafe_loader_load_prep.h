#pragma once
#include "cafe_loader_load_basics.h"
#include "cafe_loader_minfileinfo.h"
#include "cafe/kernel/cafe_kernel_process.h"

namespace cafe::loader::internal
{

int32_t
LiLoadForPrep(virt_ptr<char> moduleName,
              uint32_t moduleNameLen,
              virt_ptr<void> chunkBuffer,
              virt_ptr<LOADED_RPL> *outLoadedRpl,
              BasicLoadArgs *loadArgs,
              uint32_t unk);

int32_t
LOADER_Prep(kernel::UniqueProcessId upid,
            virt_ptr<LOADER_MinFileInfo> minFileInfo);

} // namespace cafe::loader::internal

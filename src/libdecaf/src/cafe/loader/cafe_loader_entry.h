#pragma once
#include "cafe/kernel/cafe_kernel_context.h"
#include "cafe/kernel/cafe_kernel_process.h"
#include <libcpu/be2_struct.h>

namespace cafe::loader
{

struct LOADER_EntryDispatch
{
   be2_val<uint32_t> code;
   be2_array<uint32_t, 7> args;
};
CHECK_OFFSET(LOADER_EntryDispatch, 0x00, code);
CHECK_OFFSET(LOADER_EntryDispatch, 0x04, args);
CHECK_SIZE(LOADER_EntryDispatch, 0x20);

struct LOADER_EntryParams
{
   be2_virt_ptr<kernel::Context> procContext;
   be2_val<kernel::UniqueProcessId> procId;
   be2_val<int32_t> procConfig;
   be2_virt_ptr<kernel::Context> context;
   be2_val<BOOL> interruptsAllowed;
   be2_val<uint32_t> unk0x14;
   be2_struct<LOADER_EntryDispatch> dispatch;
};
CHECK_OFFSET(LOADER_EntryParams, 0x00, procContext);
CHECK_OFFSET(LOADER_EntryParams, 0x04, procId);
CHECK_OFFSET(LOADER_EntryParams, 0x08, procConfig);
CHECK_OFFSET(LOADER_EntryParams, 0x0C, context);
CHECK_OFFSET(LOADER_EntryParams, 0x10, interruptsAllowed);
CHECK_OFFSET(LOADER_EntryParams, 0x14, unk0x14);
CHECK_OFFSET(LOADER_EntryParams, 0x18, dispatch);
CHECK_SIZE(LOADER_EntryParams, 0x38);

void
LoaderStart(BOOL isEntryCall,
            virt_ptr<LOADER_EntryParams> entryParams);

namespace internal
{

uint32_t
getProcTitleLoc();

kernel::ProcessFlags
getProcFlags();

} // namespace internal

} // namespace cafe::loader

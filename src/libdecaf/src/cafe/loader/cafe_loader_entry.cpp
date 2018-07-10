#pragma optimize("", off)
#include "cafe_loader_entry.h"
#include "cafe_loader_error.h"
#include "cafe_loader_globals.h"
#include "cafe_loader_load_shared.h"
#include "cafe_loader_log.h"

namespace cafe::loader
{

static virt_ptr<kernel::Context> gpLoaderEntry_ProcContext = nullptr;
static int32_t gpLoaderEntry_ProcConfig = -1;
static int32_t gpLoaderEntry_DispatchCode = -1;
static bool gpLoaderEntry_LoaderIntsAllowed = false;
static kernel::ProcessFlags gProcFlags = kernel::ProcessFlags::get(0);
static uint32_t gProcTitleLoc = 0;
static bool sLoaderInUserMode = true;

void
LOADER_Entry(virt_ptr<LOADER_EntryParams> entryParams)
{
   decaf_abort("Unimplemented LOADER_ENTRY");
}

void
LoaderStart(BOOL isDispatch,
            virt_ptr<LOADER_EntryParams> entryParams)
{
   sLoaderInUserMode = true;

   if (isDispatch) {
      LOADER_Entry(entryParams);
      return;
   }

   // Initialise globals
   auto kernelIpcStorage = getKernelIpcStorage();
   gpLoaderEntry_ProcContext = entryParams->procContext;
   gpLoaderEntry_ProcConfig = entryParams->procConfig;
   gpLoaderEntry_DispatchCode = entryParams->dispatch.code;
   gpLoaderEntry_LoaderIntsAllowed = !!entryParams->interruptsAllowed;
   gProcFlags = kernelIpcStorage->processFlags;
   gProcTitleLoc = kernelIpcStorage->procTitleLoc;

   // Initialise loader
   internal::initialiseSharedHeaps();

   // Clear errors
   kernelIpcStorage->fatalErr = 0;
   kernelIpcStorage->fatalMsgType = 0u;
   kernelIpcStorage->fatalLine = 0u;
   kernelIpcStorage->fatalFunction[0] = char { 0 };
   kernelIpcStorage->loaderInitError = 0;
   internal::LiResetFatalError();

   auto error =
      internal::LOADER_Init(kernelIpcStorage->targetProcessId,
                            kernelIpcStorage->numCodeAreaHeapBlocks,
                            kernelIpcStorage->maxCodeSize,
                            kernelIpcStorage->maxDataSize,
                            virt_addrof(kernelIpcStorage->rpxModule),
                            virt_addrof(kernelIpcStorage->loadedModuleList),
                            virt_addrof(kernelIpcStorage->startInfo));
   if (error) {
      if (!internal::LiGetFatalError()) {
         internal::LiSetFatalError(0x1872A7u, 0, 1, "__LoaderStart", 227);
      }

      if (!internal::LiGetFatalError()) {
         internal::LiPanic("entry.c", 239, "***RPX failed loader but didn't generate fatal error information; err={}.", error);
      }

      kernelIpcStorage->loaderInitError = error;
      kernelIpcStorage->fatalLine = internal::LiGetFatalLine();
      kernelIpcStorage->fatalErr = internal::LiGetFatalError();
      kernelIpcStorage->fatalMsgType = internal::LiGetFatalMsgType();
      std::strncpy(virt_addrof(kernelIpcStorage->fatalFunction).getRawPointer(),
                   internal::LiGetFatalFunction().data(),
                   kernelIpcStorage->fatalFunction.size() - 1);
      kernelIpcStorage->fatalFunction[kernelIpcStorage->fatalFunction.size() - 1] = char { 0 };
   }

   if (kernelIpcStorage->targetProcessId == kernel::UniqueProcessId::Root) {
      // TODO: Syscall Loader_FinishInitAndPreload
   } else {
      // TODO: Syscall Loader_ProfileEntry
      // TODO: Syscall Loader_ContinueStartProcess
   }
}

namespace internal
{

uint32_t
getProcTitleLoc()
{
   return gProcTitleLoc;
}

kernel::ProcessFlags
getProcFlags()
{
   return gProcFlags;
}

} // namespace internal

} // namespace cafe::loader

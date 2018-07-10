#include "cafe_kernel_loader.h"
#include "cafe_kernel_process.h"
#include "cafe/loader/cafe_loader_entry.h"
#include "cafe/loader/cafe_loader_globals.h"

#include <cstdint>
#include <libcpu/cpu.h>

namespace cafe::kernel::internal
{

void
KiRPLLoaderSetup(ProcessFlags processFlags,
                 UniqueProcessId callerProcessId,
                 UniqueProcessId targetProcessId)
{
   auto loaderIpc = loader::getKernelIpcStorage();
   auto contextStorage = loader::getContextStorage();

   if (targetProcessId == UniqueProcessId::Root) {
      processFlags = ProcessFlags::get(0).
         isFirstProcess(true);
   } else {
      // Clear all flags except debugLevel
      processFlags = ProcessFlags::get(0).
         debugLevel(processFlags.debugLevel());

      if (processFlags.unkBit12() || processFlags.isFirstProcess()) {
         processFlags = processFlags
            .disableSharedLibraries(true);
      }

      // TODO: Fix this when we implement multi-process
      // Normally this would only be set by Root process, but in decaf until we
      // implement processes then we will always be first process
      processFlags = processFlags.
         isFirstProcess(true);
   }

   loaderIpc->unk0x00 = 0u;
   loaderIpc->processFlags = processFlags;

   auto context = virt_ptr<Context> { nullptr };
   auto stackTop = virt_addr { 0 };

   switch (cpu::this_core::id()) {
   case 0:
      context = virt_addrof(contextStorage->context0);
      stackTop = virt_cast<virt_addr>(virt_addrof(contextStorage->stack0) + contextStorage->stack0.size() - 16);
      break;
   case 1:
      context = virt_addrof(contextStorage->context1);
      stackTop = virt_cast<virt_addr>(virt_addrof(contextStorage->stack1) + contextStorage->stack1.size() - 16);
      break;
   case 2:
      context = virt_addrof(contextStorage->context2);
      stackTop = virt_cast<virt_addr>(virt_addrof(contextStorage->stack2) + contextStorage->stack2.size() - 16);
      break;
   default:
      decaf_abort(fmt::format("Unexpected core id {}", cpu::this_core::id()));
   }

   // In a real kernel we'd switch to the PPC context
   // context->srr0 = loader entry point
   // context->srr1 = 0x4000
   context->gpr[1] = static_cast<uint32_t>(stackTop);
   context->gpr[3] = 0u;
   context->gpr[4] = static_cast<uint32_t>(virt_cast<virt_addr>(virt_addrof(loaderIpc->entryParams)));

   // But our loader is HLE only atm so we just set the current context and call loader directly
   auto kernelContext = setCurrentContext(context);
   loader::LoaderStart(FALSE, virt_addrof(loaderIpc->entryParams));
   setCurrentContext(kernelContext);
}

void
KiRPLStartup(UniqueProcessId callerProcessId,
             UniqueProcessId targetProcessId,
             ProcessFlags processFlags,
             uint32_t numCodeAreaHeapBlocks,
             uint32_t maxCodeSize,
             uint32_t maxDataSize,
             uint32_t titleLoc)
{
   auto loaderIpc = loader::getKernelIpcStorage();
   loaderIpc->maxDataSize = maxDataSize;
   loaderIpc->callerProcessId = callerProcessId;
   loaderIpc->maxCodeSize = maxCodeSize;
   loaderIpc->procTitleLoc = titleLoc;
   loaderIpc->targetProcessId = targetProcessId;
   loaderIpc->numCodeAreaHeapBlocks = numCodeAreaHeapBlocks;
   loaderIpc->rpxModule = nullptr;
   loaderIpc->loadedModuleList = nullptr;
   loaderIpc->unk0x28 = 0u;

   loaderIpc->entryParams.procContext = nullptr;
   loaderIpc->entryParams.procId = UniqueProcessId::Invalid;
   loaderIpc->entryParams.procConfig = -1;
   loaderIpc->entryParams.context = nullptr;
   loaderIpc->entryParams.interruptsAllowed = FALSE;
   std::memset(virt_addrof(loaderIpc->entryParams.dispatch).getRawPointer(),
               0, sizeof(loader::LOADER_EntryDispatch));

   KiRPLLoaderSetup(processFlags, callerProcessId, targetProcessId);
}

} // namespace cafe::kernel::internal

#include "coreinit.h"
#include "coreinit_ghs.h"
#include <common/log.h>

namespace cafe::coreinit
{

static void
rpl_entry(/* no args for coreinit entry point */)
{
   gLog->error("Coreinit entry point unimplemented");
   exit(-1);
}

void
Library::registerSymbols()
{
   RegisterEntryPoint(rpl_entry);

   registerAlarmSymbols();
   registerAppIoSymbols();
   registerAtomicSymbols();
   registerAtomic64Symbols();
   registerBspSymbols();
   registerCacheSymbols();
   registerClipboardSymbols();
   registerCoreSymbols();
   registerCoroutineSymbols();
   registerCosReportSymbols();
   registerDeviceSymbols();
   registerDriverSymbols();
   registerDynLoadSymbols();
   registerEventSymbols();
   registerExceptionSymbols();
   registerFastMutexSymbols();
   registerFiberSymbols();
   registerFsSymbols();
   registerFsClientSymbols();
   registerFsCmdSymbols();
   registerFsCmdBlockSymbols();
   registerFsDriverSymbols();
   registerFsStateMachineSymbols();
   registerFsaSymbols();
   registerFsaShimSymbols();
   registerGhsSymbols();
   registerHandleSymbols();
   registerImSymbols();
   registerInterruptSymbols();
   registerIosSymbols();
   registerIpcBufPoolSymbols();
   registerIpcDriverSymbols();
   registerLockedCacheSymbols();
   registerMcpSymbols();
   registerMemAllocatorSymbols();
   registerMemBlockHeapSymbols();
   registerMemDefaultHeapSymbols();
   registerMemExpHeapSymbols();
   registerMemFrmHeapSymbols();
   registerMemHeapSymbols();
   registerMemListSymbols();
   registerMemorySymbols();
   registerMemUnitHeapSymbols();
   registerMessageQueueSymbols();
   registerMutexSymbols();
   registerOsReportSymbols();
   registerOverlayArenaSymbols();
   registerRendezvousSymbols();
   registerSchedulerSymbols();
   registerScreenSymbols();
   registerSemaphoreSymbols();
   registerSnprintfSymbols();
   registerSpinLockSymbols();
   registerSystemHeapSymbols();
   registerSystemInfoSymbols();
   registerTaskQueueSymbols();
   registerThreadSymbols();
   registerTimeSymbols();
   registerUserConfigSymbols();
}

} // namespace cafe::coreinit

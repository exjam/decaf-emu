#include "cafe_kernel_context.h"
#include "cafe_kernel_exception.h"
#include "cafe_kernel_heap.h"
#include "cafe_kernel_ipckdriver.h"
#include "cafe/cafe_ppc_interface_invoke.h"

#include "decaf_config.h"
#include "debugger/debugger.h"
#include "cafe/libraries/coreinit/coreinit_alarm.h"
#include "cafe/libraries/coreinit/coreinit_interrupts.h"
#include "cafe/libraries/coreinit/coreinit_scheduler.h"
#include "cafe/libraries/gx2/gx2_event.h"

#include <array>
#include <common/log.h>
#include <common/platform_stacktrace.h>
#include <common/platform_thread.h>
#include <libcpu/cpu.h>
#include <vector>
#include <queue>

namespace cafe::kernel
{

constexpr auto ExceptionThreadStackSize = 0x100u;

struct StaticExceptionData
{
   be2_array<Context, 3> exceptionThreadContext;
   be2_array<std::byte, ExceptionThreadStackSize * 3> exceptionStackBuffer;
};

static virt_ptr<StaticExceptionData>
sExceptionData;

namespace internal
{

static void
handleCpuInterrupt(cpu::Core *core,
                   uint32_t flags)
{
   if (flags & cpu::SRESET_INTERRUPT) {
      platform::exitThread(0);
   }

   if (flags & cpu::DBGBREAK_INTERRUPT) {
      if (decaf::config::debugger::enabled) {
         coreinit::internal::pauseCoreTime(true);
         debugger::handleDebugBreakInterrupt();
         coreinit::internal::pauseCoreTime(false);
      }
   }

   auto unsafeInterrupts = cpu::NONMASKABLE_INTERRUPTS | cpu::DBGBREAK_INTERRUPT;
   if (!(flags & ~unsafeInterrupts)) {
      // Due to the fact that non-maskable interrupts are not able to be disabled
      // it is possible the application has the scheduler lock or something, so we
      // need to stop processing here or else bad things could happen.
      return;
   }

   // We need to disable the scheduler while we handle interrupts so we
   // do not reschedule before we are done with our interrupts.  We disable
   // interrupts if they were on so any PPC callbacks executed do not
   // immediately and reentrantly interrupt.  We also make sure did not
   // interrupt someone who disabled the scheduler, since that should never
   // happen and will cause bugs.

   decaf_check(coreinit::internal::isSchedulerEnabled());

   auto exceptionContext = virt_addrof(sExceptionData->exceptionThreadContext[core->id]);
   auto interruptedContext = setCurrentContext(exceptionContext);

   auto originalInterruptState = coreinit::OSDisableInterrupts();
   coreinit::internal::disableScheduler();

   if (flags & cpu::ALARM_INTERRUPT) {
      coreinit::internal::handleAlarmInterrupt(interruptedContext);
   }

   if (flags & cpu::GPU_RETIRE_INTERRUPT) {
      gx2::internal::handleGpuRetireInterrupt();
   }

   if (flags & cpu::GPU_FLIP_INTERRUPT) {
      gx2::internal::handleGpuFlipInterrupt();
   }

   if (flags & cpu::IPC_INTERRUPT) {
      ipcDriverKernelHandleInterrupt();
   }

   coreinit::internal::enableScheduler();
   coreinit::OSRestoreInterrupts(originalInterruptState);

   // Restore the interrupted context and return to continue execution!
   setCurrentContext(interruptedContext);

   // We must never receive an interrupt while processing a kernel
   // function as if the scheduler is locked, we are in for some shit.
   coreinit::internal::lockScheduler();
   coreinit::internal::checkRunningThreadNoLock(false);
   coreinit::internal::unlockScheduler();
}

struct FaultData
{
   enum class Reason
   {
      Unknown,
      Segfault,
      IllegalInstruction,
   };

   Reason reason;
   uint32_t address;
   platform::StackTrace *stackTrace = nullptr;
};

static void
faultFiberEntryPoint(void *param)
{
   auto faultData = reinterpret_cast<FaultData *>(param);

   // We may have been in the middle of a kernel function...
   if (coreinit::internal::isSchedulerLocked()) {
      coreinit::internal::unlockScheduler();
   }

   // Move back an instruction so we can re-exucute the failed instruction
   //  and so that the debugger shows the right stop point.
   cpu::this_core::state()->nia -= 4;

   // If the decaf debugger is enabled, we will catch this exception there
   if (decaf::config::debugger::enabled) {
      coreinit::internal::pauseCoreTime(true);
      debugger::handleDebugBreakInterrupt();
      coreinit::internal::pauseCoreTime(false);

      // This will shut down the thread and reschedule.  This is required
      //  since returning from the segfault handler is an error.
      coreinit::OSExitThread(0);
   }

   auto core = cpu::this_core::state();
   decaf_assert(core, "Uh oh? CPU fault Handler with invalid core");

   // Log the core state
   fmt::MemoryWriter out;
   out.write("nia: 0x{:08x}\n", core->nia);
   out.write("lr: 0x{:08x}\n", core->lr);
   out.write("cr: 0x{:08x}\n", core->cr.value);
   out.write("ctr: 0x{:08x}\n", core->ctr);
   out.write("xer: 0x{:08x}\n", core->xer.value);

   for (auto i = 0u; i < 32; ++i) {
      out.write("gpr[{}]: 0x{:08x}\n", i, core->gpr[i]);
   }

   auto coreState = out.str();
   gLog->critical("{}", coreState);

   // Handle the host fault
   if (faultData->reason == FaultData::Reason::Segfault) {
      decaf_host_fault(fmt::format("Invalid memory access for address 0x{:08X} at 0x{:08X}\n",
                                   faultData->address, core->nia),
                       faultData->stackTrace);
   } else if (faultData->reason == FaultData::Reason::IllegalInstruction) {
      decaf_host_fault(fmt::format("Invalid instruction at address 0x{:08X}\n", faultData->address),
                       faultData->stackTrace);
   } else {
      decaf_host_fault(fmt::format("Unexpected fault occured, fault reason was {} at 0x{:08X}\n",
                                   static_cast<int32_t>(faultData->reason), core->nia),
                       faultData->stackTrace);
   }
}

static void
handleCpuSegfault(cpu::Core *core,
                  uint32_t address)
{
   auto hostStackTrace = platform::captureStackTrace();
   auto faultData = new FaultData { };
   faultData->reason = FaultData::Reason::Segfault;
   faultData->address = address;
   faultData->stackTrace = hostStackTrace;
   resetFaultedContextFiber(getCurrentContext(), faultFiberEntryPoint, faultData);
}

static void
handleCpuIllegalInstruction(cpu::Core *core)
{
   auto hostStackTrace = platform::captureStackTrace();
   auto faultData = new FaultData { };
   faultData->reason = FaultData::Reason::IllegalInstruction;
   faultData->address = core->nia;
   faultData->stackTrace = hostStackTrace;
   resetFaultedContextFiber(getCurrentContext(), faultFiberEntryPoint, faultData);
}

void
initialiseExceptionContext(cpu::Core *core)
{
   auto context = virt_addrof(sExceptionData->exceptionThreadContext[core->id]);
   auto stack = virt_addrof(sExceptionData->exceptionStackBuffer[ExceptionThreadStackSize * 3]);

   memset(context.getRawPointer(), 0, sizeof(Context));
   context->gpr[1] = virt_cast<virt_addr>(stack).getAddress() + ExceptionThreadStackSize;
}

void
initialiseExceptionHandlers()
{
   cpu::setInterruptHandler(&handleCpuInterrupt);
   cpu::setSegfaultHandler(&handleCpuSegfault);
   cpu::setIllInstHandler(&handleCpuIllegalInstruction);
}

void
initialiseStaticExceptionData()
{
   sExceptionData = allocStaticData<StaticExceptionData>();
}

} // namespace internal

} // namespace cafe::kernel

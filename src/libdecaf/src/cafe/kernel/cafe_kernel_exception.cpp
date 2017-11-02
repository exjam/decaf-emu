#include "cafe_kernel.h"
#include "cafe_kernel_context.h"
#include "cafe_kernel_exception.h"
#include "cafe_kernel_ipc.h"
#include "cafe_kernel_process.h"
#include "cafe/cafe_ppc_interface_invoke.h"

#include <array>
#include <common/log.h>
#include <common/platform_stacktrace.h>
#include <libcpu/cpu.h>
#include <vector>
#include <queue>

namespace cafe::kernel
{

constexpr auto ExceptionThreadStackSize = 0x100u;

struct UserExceptionHandler
{
   ExceptionType type;
   ExceptionHandler handler;
   virt_ptr<Context> context;
   virt_ptr<void> stack;
};

struct UserExceptionCallback
{
   UserExceptionCallbackFn callback;
   virt_ptr<Context> context;
   virt_ptr<void> stack;
};

struct KernelExceptionCallback
{
   KernelExceptionCallbackFn callback;
   uint32_t argc;
   virt_ptr<void> argv;
};

struct ProcessExceptionData
{
   std::vector<UserExceptionHandler> handlers;
};

struct StaticExceptionData
{
   be2_array<Context, 3> exceptionThreadContext;
   be2_array<std::byte, ExceptionThreadStackSize * 3> exceptionStackBuffer;
};

static virt_ptr<StaticExceptionData>
sExceptionData;

static std::array<ProcessExceptionData, NumProcesses>
sProcessExceptionData;

static std::array<ExceptionHandler, static_cast<std::size_t>(ExceptionType::Max)>
sKernelExceptionHandlers;

static std::queue<KernelExceptionCallback>
sKernelPendingCallbacks;

static std::queue<UserExceptionCallback>
sUserPendingCallbacks;

void
registerUserExceptionHandler(ExceptionType type,
                             ExceptionHandler handler,
                             virt_ptr<Context> context,
                             virt_ptr<void> stack)
{
   auto pidx = static_cast<std::size_t>(getRampid());
   auto &processExceptionData = sProcessExceptionData[pidx];
   processExceptionData.handlers.emplace_back(type, handler, context, stack);
}

namespace internal
{

void
registerKernelExceptionHandler(ExceptionType type,
                               ExceptionHandler handler)
{
   sKernelExceptionHandlers[static_cast<std::size_t>(type)] = handler;
}

void
queueKernelExceptionCallback(KernelExceptionCallbackFn callback,
                             uint32_t argc,
                             virt_ptr<void> argv)
{
   sKernelPendingCallbacks.emplace(callback, argc, argv);
}

void
queueUserExceptionCallback(UserExceptionCallbackFn callback,
                           virt_ptr<Context> context,
                           virt_ptr<void> stack)
{
   sUserPendingCallbacks.emplace(callback, context, stack);
}

static void
unhandledException(ExceptionType type,
                   virt_ptr<Context> interruptedContext,
                   virt_ptr<Context> currentContext)
{
}

static void
handleSystemResetException(cpu::Core *core)
{

}

static void
callExceptionHandler(ExceptionType type,
                     virt_ptr<Context> interruptedContext,
                     virt_ptr<Context> currentContext)
{
   auto handler = sKernelExceptionHandlers[static_cast<std::size_t>(type)];
   auto kernelHandled = false;
   if (!handler) {
      handler(interruptedContext, currentContext);
      kernelHandled = true;
   }

   auto pidx = static_cast<std::size_t>(getRampid());
   auto userHandled = false;
   for (auto &handler : sProcessExceptionData[pidx].handlers) {
      handler.context->gpr[1] = static_cast<uint32_t>(virt_cast<virt_addr>(handler.stack));
      setCurrentContext(handler.context);
      handler.handler(interruptedContext, handler.context);
      userHandled = true;
   }

   // If we called a user handler then we must restore the kernel context
   if (userHandled) {
      setCurrentContext(currentContext);
   }

   // If no one handled it then call the unhandled exception handler
   if (!kernelHandled && !userHandled) {
      unhandledException(type, interruptedContext, currentContext);
   }
}

static void
handleCpuInterrupt(cpu::Core *core,
                   uint32_t flags)
{
   auto exceptionContext = virt_addrof(sExceptionData->exceptionThreadContext[core->id]);
   auto interruptedContext = setCurrentContext(exceptionContext);

   if (flags & cpu::SRESET_INTERRUPT) {
      callExceptionHandler(ExceptionType::SystemReset, interruptedContext, exceptionContext);
   }

   // TODO: This should be a ExceptionType::ExternalInterrupt
   if (flags & cpu::IPC_INTERRUPT) {
      cafe::kernel::handleIpcInterrupt();
   }

   // TODO: This should be a ExceptionType::ExternalInterrupt
   if (flags & cpu::GPU_RETIRE_INTERRUPT) {
      // TODO: This is wired up through tcl.rpl / tve cant remember
   }

   // TODO: This should be a ExceptionType::ExternalInterrupt
   if (flags & cpu::GPU_FLIP_INTERRUPT) {
      // TODO: This is wired up through tcl.rpl / tve cant remember
   }

   if (flags & cpu::ALARM_INTERRUPT) {
      callExceptionHandler(ExceptionType::Decrementer, interruptedContext, exceptionContext);
   }

   if (flags & cpu::DBGBREAK_INTERRUPT) {
      callExceptionHandler(ExceptionType::Breakpoint, interruptedContext, exceptionContext);
   }

   // Call any queued kernel callbacks
   while (!sKernelPendingCallbacks.empty()) {
      auto callback = sKernelPendingCallbacks.front();
      sKernelPendingCallbacks.pop();
      callback.callback(callback.argc, callback.argv);
   }

   // Finally call any queued user exception callbacks
   while (!sUserPendingCallbacks.empty()) {
      auto callback = sUserPendingCallbacks.front();
      sUserPendingCallbacks.pop();

      callback.context->gpr[1] = static_cast<uint32_t>(virt_cast<virt_addr>(callback.stack));
      setCurrentContext(callback.context);
      cafe::invoke(core, callback.callback, interruptedContext, callback.context);
   }

   // Restore the interrupted context and return to continue execution!
   setCurrentContext(interruptedContext);

   // TODO: This is a decaf specific interrupt telling us to wakeup the
   // current core because something is ready to run!
   if (flags & cpu::GENERIC_INTERRUPT) {
   }


   /*
   if (interrupt_flags & cpu::DBGBREAK_INTERRUPT) {
      if (decaf::config::debugger::enabled) {
         coreinit::internal::pauseCoreTime(true);
         debugger::handleDebugBreakInterrupt();
         coreinit::internal::pauseCoreTime(false);
      }
   }

   auto unsafeInterrupts = cpu::NONMASKABLE_INTERRUPTS | cpu::DBGBREAK_INTERRUPT;
   if (!(interrupt_flags & ~unsafeInterrupts)) {
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

   coreinit::OSContext savedContext;
   kernel::saveContext(&savedContext);
   kernel::restoreContext(&sInterruptContext[cpu::this_core::id()]);

   auto originalInterruptState = coreinit::OSDisableInterrupts();
   coreinit::internal::disableScheduler();

   auto interruptedThread = coreinit::internal::getCurrentThread();

   if (interrupt_flags & cpu::ALARM_INTERRUPT) {
      coreinit::internal::handleAlarmInterrupt(&interruptedThread->context);
   }

   if (interrupt_flags & cpu::GPU_RETIRE_INTERRUPT) {
      gx2::internal::handleGpuRetireInterrupt();
   }

   if (interrupt_flags & cpu::GPU_FLIP_INTERRUPT) {
      gx2::internal::handleGpuFlipInterrupt();
   }

   if (interrupt_flags & cpu::IPC_INTERRUPT) {
      cafe::kernel::handleIpcInterrupt();
   }

   coreinit::internal::enableScheduler();
   coreinit::OSRestoreInterrupts(originalInterruptState);

   kernel::restoreContext(&savedContext);

   // We must never receive an interrupt while processing a kernel
   // function as if the scheduler is locked, we are in for some shit.
   coreinit::internal::lockScheduler();
   coreinit::internal::checkRunningThreadNoLock(false);
   coreinit::internal::unlockScheduler();
   */
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

#if 0
   // We may have been in the middle of a kernel function...
   if (coreinit::internal::isSchedulerLocked()) {
      coreinit::internal::unlockScheduler();
   }
#endif

   // Move back an instruction so we can re-exucute the failed instruction
   //  and so that the debugger shows the right stop point.
   cpu::this_core::state()->nia -= 4;

#if 0
   // If the decaf debugger is enabled, we will catch this exception there
   if (decaf::config::debugger::enabled) {
      coreinit::internal::pauseCoreTime(true);
      debugger::handleDebugBreakInterrupt();
      coreinit::internal::pauseCoreTime(false);

      // This will shut down the thread and reschedule.  This is required
      //  since returning from the segfault handler is an error.
      coreinit::OSExitThread(0);
   }
#endif

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
                                   faultData->reason, core->nia),
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

/*
0, "KERNEL_INTERRUPT_ERROR", 0, 0x80000001, "KERNEL_INT_CTRL_PI_ALL", 0x1,
1, "KERNEL_INTERRUPT_DSP", 1, 0x80000001, "KERNEL_INT_CTRL_PI_ALL", 0x40,
2, "KERNEL_INTERRUPT_GPU7", 0, 0x20000003, "KERNEL_INT_CTRL_PI_LATTE", 0x800000,
3, "KERNEL_INTERRUPT_GPIPPC", 0, 0x80000009, "KERNEL_INT_CTRL_AHB_ALL", 0x400,
4, "KERNEL_INTERRUPT_PRIMARY_I2C", 0, 0x2000000B, "KERNEL_INT_CTRL_AHB_LATTE", 0x2000,
5, "KERNEL_INTERRUPT_DSP_AI", 0, 0x80000007, "KERNEL_INT_CTRL_DSP_ALL", 0x8,
6, "KERNEL_INTERRUPT_DSP_AI2", 0, 0x20000008, "KERNEL_INT_CTRL_DSP_LATTE", 0x1000,
7, "KERNEL_INTERRUPT_DSP_ACC", 0, 0x80000007, "KERNEL_INT_CTRL_DSP_ALL", 0x20,
8, "KERNEL_INTERRUPT_DSP_DSP", 0, 0x80000007, "KERNEL_INT_CTRL_DSP_ALL", 0x80,
9, "KERNEL_INTERRUPT_IPC_PPC0", 0, 0x2000000B, "KERNEL_INT_CTRL_AHB_LATTE", 0x40000000,
0xA, "KERNEL_INTERRUPT_IPC_PPC1", 0, 0x2000000B, "KERNEL_INT_CTRL_AHB_LATTE", 0x10000000,
0xB, "KERNEL_INTERRUPT_IPC_PPC2", 0, 0x2000000B, "KERNEL_INT_CTRL_AHB_LATTE", 0x4000000,
0xC, "KERNEL_INTERRUPT_AHB", 2, 0x20000003, "KERNEL_INT_CTRL_PI_LATTE", 0x1000000,
*/

void
initialiseExceptionContext(cpu::Core *core)
{
   auto context = virt_addrof(sExceptionData->exceptionThreadContext[core->id]);
   auto stack = virt_addrof(sExceptionData->exceptionStackBuffer[ExceptionThreadStackSize * 3]);
   context->gpr[1] = virt_cast<virt_addr>(stack).getAddress() + ExceptionThreadStackSize;

   // TODO: Do we need to initialise any other registers?
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
   sExceptionData = allocStatic<StaticExceptionData>();
   sKernelExceptionHandlers.fill(nullptr);
   sProcessExceptionData.fill({ });
}

} // namespace internal

} // namespace cafe::kernel

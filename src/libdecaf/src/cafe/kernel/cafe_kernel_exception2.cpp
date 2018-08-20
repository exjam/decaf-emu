#include "cafe_kernel_context.h"
#include "cafe_kernel_exception2.h"

#include <array>
#include <functional>
#include <libcpu/cpu.h>

namespace cafe::kernel
{

void
KeSetUserModeExceptionHandler(ExceptionType type,
                              virt_ptr<UserModeExceptionHandler> handler,
                              virt_ptr<UserModeExceptionHandler> outChainInfo,
                              BOOL unk_r6)
{
}

namespace internal
{

constexpr auto ExceptionThreadStackSize = 0x1000u;

struct StaticExceptionData
{
   be2_array<Context, 3> exceptionThreadContext;
   be2_array<std::byte, ExceptionThreadStackSize * 3> exceptionStackBuffer;
};

static virt_ptr<StaticExceptionData>
sExceptionData;

static std::array<KernelExceptionHandler, static_cast<size_t>(ExceptionType::Max)>
sExceptionHandlers;

static void
defaultExceptionHandler(ExceptionType type,
                        virt_ptr<Context> interruptedContext)
{
}

static void
handleUnhandledProgramException(ExceptionType type,
                                virt_ptr<Context> interruptedContext)
{
}

static void
handleSystemResetException(ExceptionType type,
                                virt_ptr<Context> interruptedContext)
{
}

static void
handleCpuSegfault(cpu::Core *core,
                  uint32_t address)
{
   /*
   auto hostStackTrace = platform::captureStackTrace();
   auto faultData = new FaultData { };
   faultData->reason = FaultData::Reason::Segfault;
   faultData->address = address;
   faultData->stackTrace = hostStackTrace;
   resetFaultedContextFiber(getCurrentContext(), faultFiberEntryPoint, faultData);
   */
}

static void
handleCpuIllegalInstruction(cpu::Core *core)
{
   /*
   auto hostStackTrace = platform::captureStackTrace();
   auto faultData = new FaultData { };
   faultData->reason = FaultData::Reason::IllegalInstruction;
   faultData->address = core->nia;
   faultData->stackTrace = hostStackTrace;
   resetFaultedContextFiber(getCurrentContext(), faultFiberEntryPoint, faultData);
   */
}

static void
handleCpuInterrupt(cpu::Core *core,
                   uint32_t flags)
{
   // Switch to exception context!
   auto exceptionContext = virt_addrof(sExceptionData->exceptionThreadContext[core->id]);
   auto interruptedContext = setCurrentContext(exceptionContext);
   auto callExceptionHandler = [](ExceptionType type, virt_ptr<Context> interruptedContext) {
         sExceptionHandlers[static_cast<size_t>(type)](type, interruptedContext);
      };

   if (flags & cpu::ExceptionFlags::SystemResetException) {
      callExceptionHandler(ExceptionType::SystemReset, interruptedContext);
   }

   if (flags & cpu::ExceptionFlags::MachineCheckException) {
      callExceptionHandler(ExceptionType::MachineCheck, interruptedContext);
   }

   if (flags & cpu::ExceptionFlags::DSIException) {
      callExceptionHandler(ExceptionType::DSI, interruptedContext);
   }

   if (flags & cpu::ExceptionFlags::ISIException) {
      callExceptionHandler(ExceptionType::ISI, interruptedContext);
   }

   if (flags & cpu::ExceptionFlags::ExternalInterruptException) {
      callExceptionHandler(ExceptionType::ExternalInterrupt, interruptedContext);
   }

   if (flags & cpu::ExceptionFlags::AlignmentException) {
      callExceptionHandler(ExceptionType::Alignment, interruptedContext);
   }

   if (flags & cpu::ExceptionFlags::ProgramException) {
      callExceptionHandler(ExceptionType::Program, interruptedContext);
   }

   if (flags & cpu::ExceptionFlags::FloatingPointException) {
      callExceptionHandler(ExceptionType::FloatingPoint, interruptedContext);
   }

   if (flags & cpu::ExceptionFlags::DecrementerException) {
      callExceptionHandler(ExceptionType::Decrementer, interruptedContext);
   }

   if (flags & cpu::ExceptionFlags::SystemCallException) {
      callExceptionHandler(ExceptionType::SystemCall, interruptedContext);
   }

   if (flags & cpu::ExceptionFlags::TraceException) {
      callExceptionHandler(ExceptionType::Trace, interruptedContext);
   }

   if (flags & cpu::ExceptionFlags::PerformanceMonitorException) {
      callExceptionHandler(ExceptionType::PerformanceMonitor, interruptedContext);
   }

   if (flags & cpu::ExceptionFlags::BreakpointException) {
      callExceptionHandler(ExceptionType::Breakpoint, interruptedContext);
   }
   /*
   ? ? ? ?
   ExceptionType::SystemInterrupt
   ExceptionType::ICI
   */

   // Return to the previous context and continue execution
   setCurrentContext(interruptedContext);
}

void
initialiseExceptionHandlers()
{
   sExceptionHandlers.fill(defaultExceptionHandler);

   setKernelExceptionHandler(ExceptionType::SystemReset,
                             handleSystemResetException);

   setKernelExceptionHandler(ExceptionType::DSI,
                             handleUnhandledProgramException);
   setKernelExceptionHandler(ExceptionType::ISI,
                             handleUnhandledProgramException);
   setKernelExceptionHandler(ExceptionType::Program,
                             handleUnhandledProgramException);

   cpu::setInterruptHandler(&handleCpuInterrupt);
   cpu::setSegfaultHandler(&handleCpuSegfault);
   cpu::setIllInstHandler(&handleCpuIllegalInstruction);
}

void
setKernelExceptionHandler(ExceptionType type,
                          KernelExceptionHandler handler)
{
   sExceptionHandlers[static_cast<size_t>(type)] = handler;
}

} // namespace internal

} // namespace cafe::kernel

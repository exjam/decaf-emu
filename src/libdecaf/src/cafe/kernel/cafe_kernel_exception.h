#pragma once
#include "cafe_kernel_context.h"
#include "cafe_kernel_process.h"
#include <libcpu/be2_struct.h>
#include <libcpu/cpu.h>

namespace cafe::kernel
{

enum class ExceptionType
{
   SystemReset = 0,
   MachineCheck = 1,
   DSI = 2,
   ISI = 3,
   ExternalInterrupt = 4,
   Alignment = 5,
   Program = 6,
   FloatingPoint = 7,
   Decrementer = 8,
   SystemCall = 9,
   Trace = 10,
   PerformanceMonitor = 11,
   Breakpoint = 12,
   SystemInterrupt = 13,
   ICI = 14,
   Max,
};

enum class ExternalInterruptType
{
   Error = 0,
   Dsp = 1,
   Gpu7 = 2,
   GpiPpc = 3,
   PrimaryI2C = 4,
   DspAi = 5,
   DspAi2 = 6,
   DspAcc= 7,
   DspDsp = 8,
   IpcPpc0 = 9,
   IpcPpc1 = 10,
   IpcPpc2 = 11,
   Ahb = 12,
   Max,
};

using ExceptionHandler = std::function<void(virt_ptr<Context> interruptedContext,
                                            virt_ptr<Context> exceptionContext)>;


using KernelExceptionCallbackFn = std::function<void(uint32_t arg1,
                                                     uint32_t arg2,
                                                     uint32_t arg3,
                                                     uint32_t arg4,
                                                     uint32_t arg5,
                                                     uint32_t arg6)>;

using UserExceptionCallbackFn = virt_func_ptr<void(virt_ptr<Context> interruptedContext,
                                                   virt_ptr<Context> interruptContext)>;

void
registerUserExceptionHandler(ExceptionType type,
                             ExceptionHandler handler,
                             virt_ptr<Context> context,
                             virt_ptr<void> stack);
namespace internal
{

void
registerKernelExceptionHandler(ExceptionType type,
                               ExceptionHandler handler);

void
queueKernelExceptionCallback(KernelExceptionCallbackFn callback,
                             uint32_t arg1,
                             uint32_t arg2,
                             uint32_t arg3,
                             uint32_t arg4,
                             uint32_t arg5,
                             uint32_t arg6);

void
queueUserExceptionCallback(UserExceptionCallbackFn callback,
                           virt_ptr<Context> context,
                           virt_ptr<void> stack);

void
initialiseExceptionContext(cpu::Core *core);

void
initialiseExceptionHandlers();

void
initialiseStaticExceptionData();

} // namespace internal

} // namespace cafe::kernel

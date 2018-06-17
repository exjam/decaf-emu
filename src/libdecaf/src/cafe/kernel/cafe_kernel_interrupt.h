#pragma once
#include "cafe_kernel_context.h"
#include "cafe_kernel_process.h"

#include <cstdint>
#include <functional>
#include <libcpu/be2_struct.h>

namespace cafe::kernel
{

enum class InterruptType
{
   Error = 0,
   Dsp = 1,
   Gpu7 = 2,
   GpiPpc = 3,
   PrimaryI2C = 4,
   DspAi = 5,
   DspAi2 = 6,
   DspAcc = 7,
   DspDsp = 8,
   IpcPpc0 = 9,
   IpcPpc1 = 10,
   IpcPpc2 = 11,
   Ahb = 12,
   Max,
};

using UserInterruptHandler = virt_func_ptr<
   void(InterruptType interrupt,
        virt_ptr<Context> interruptedContext,
        virt_ptr<void> userData,
        virt_ptr<Context> currentContext)>;

void
KeDisableInterrupt(InterruptType interrupt);

void
KeEnableInterrupt(InterruptType interrupt);

void
KeClearAndEnableInterrupt(InterruptType interrupt);

uint32_t
KeGetInterruptStatus(InterruptType interrupt);

uint32_t
KeClearInterruptStatus(InterruptType interrupt);

virt_ptr<void>
KeGetInterruptHandler(InterruptType interrupt);

virt_ptr<void>
KeSetInterruptHandler(InterruptType interrupt,
                      UserInterruptHandler callback,
                      virt_ptr<void> callbackUserData,
                      virt_ptr<void> stack,
                      virt_ptr<Context> context);

namespace internal
{

using KernelInterruptHandler = std::function<
   void(InterruptType interrupt,
        virt_ptr<Context> interruptedContext,
        virt_ptr<Context> exceptionContext)>;

virt_ptr<void>
setUserInterruptHandler(InterruptType interrupt,
                        UserInterruptHandler callback,
                        virt_ptr<void> callbackUserData,
                        virt_ptr<void> stack,
                        virt_ptr<Context> context,
                        RamProcessId ramPid);

KernelInterruptHandler
setKernelInterruptHandler(InterruptType interrupt,
                          KernelInterruptHandler handler);

bool
clearAndEnableInterrupt(InterruptType interrupt,
                        RamProcessId ramPid);

} // namespace internal

} // namespace cafe::kernel

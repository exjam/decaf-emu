#include "cafe_kernel_interrupt.h"
#include "cafe_kernel_lock.h"
#include "cafe_kernel_process.h"
#include "cafe_kernel_exception2.h"
#include <array>
#include <libcpu/cpu.h>
#include <gsl/gsl-lite.h>

namespace cafe::kernel
{

enum InterruptRegisterType : uint32_t
{
   PiLatte = 0x20000003,
   DspLatte = 0x20000008,
   AhbLatte = 0x2000000B,

   PiAll = 0x80000001,
   DspAll = 0x80000007,
   AhbAll = 0x80000009,
};

enum PiAllFlags : uint32_t
{
   PiAllError =    0x1,
   PiAllError =   0x40,
};

enum PiLatteFlags : uint32_t
{
   PiLatteGpu7 =  0x800000,
   PiLatteAhb  = 0x1000000,
};

enum DspAllFlags : uint32_t
{
   DspAllAi  =  0x8,
   DspAllAcc = 0x20,
   DspAllDsp = 0x80,
};

enum DspLatteFlags : uint32_t
{
   DspLatteAi2 = 0x1000,
};

enum AhbAllFlags : uint32_t
{
   AhbAllGpiPpc = 0x400,
};

enum AhbLatteFlags : uint32_t
{
   AhbLatteIpcPpc0 = 0x40000000,
   AhbLatteIpcPpc1 = 0x10000000,
   AhbLatteIpcPpc2 =  0x4000000,
};

struct InterruptInfo
{
   InterruptType interrupt;
   const char *name;
   uint32_t unknown_0x8;
   InterruptRegisterType ctrlReg;
   const char *ctrlRegName;
   uint32_t mask;
};

/*

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
*/
std::array<InterruptInfo, 4> x { {
   { InterruptType::Error,       "KERNEL_INTERRUPT_ERROR",        0, InterruptRegisterType::PiAll,    "KERNEL_INT_CTRL_PI_ALL",            0x1 },
   { InterruptType::Dsp,         "KERNEL_INTERRUPT_DSP",          1, InterruptRegisterType::PiAll,    "KERNEL_INT_CTRL_PI_ALL",           0x40 },
   { InterruptType::Gpu7,        "KERNEL_INTERRUPT_GPU7",         0, InterruptRegisterType::PiLatte,  "KERNEL_INT_CTRL_PI_LATTE",     0x800000 },
   { InterruptType::Ahb,         "KERNEL_INTERRUPT_AHB",          2, InterruptRegisterType::PiLatte,  "KERNEL_INT_CTRL_PI_LATTE",    0x1000000 },
} };

std::array<InterruptInfo, 4> y { {
   { InterruptType::DspAi,       "KERNEL_INTERRUPT_DSP_AI",       0, InterruptRegisterType::DspAll,   "KERNEL_INT_CTRL_DSP_ALL",           0x8 },
   { InterruptType::DspAi2,      "KERNEL_INTERRUPT_DSP_AI2",      0, InterruptRegisterType::DspLatte, "KERNEL_INT_CTRL_DSP_LATTE",      0x1000 },
   { InterruptType::DspAcc,      "KERNEL_INTERRUPT_DSP_ACC",      0, InterruptRegisterType::DspAll,   "KERNEL_INT_CTRL_DSP_ALL",          0x20 },
   { InterruptType::DspDsp,      "KERNEL_INTERRUPT_DSP_DSP",      0, InterruptRegisterType::DspAll,   "KERNEL_INT_CTRL_DSP_ALL",          0x80 },
} };

std::array<InterruptInfo, 5> z { {
   { InterruptType::IpcPpc0,     "KERNEL_INTERRUPT_IPC_PPC0",     0, InterruptRegisterType::AhbLatte, "KERNEL_INT_CTRL_AHB_LATTE",  0x40000000 },
   { InterruptType::IpcPpc1,     "KERNEL_INTERRUPT_IPC_PPC1",     0, InterruptRegisterType::AhbLatte, "KERNEL_INT_CTRL_AHB_LATTE",  0x10000000 },
   { InterruptType::IpcPpc2,     "KERNEL_INTERRUPT_IPC_PPC2",     0, InterruptRegisterType::AhbLatte, "KERNEL_INT_CTRL_AHB_LATTE",   0x4000000 },
   { InterruptType::GpiPpc,      "KERNEL_INTERRUPT_GPIPPC",       0, InterruptRegisterType::AhbAll,   "KERNEL_INT_CTRL_AHB_ALL",         0x400 },
   { InterruptType::PrimaryI2C,  "KERNEL_INTERRUPT_PRIMARY_I2C",  0, InterruptRegisterType::AhbLatte, "KERNEL_INT_CTRL_AHB_LATTE",      0x2000 },
} };

gsl::span<InterruptInfo> w[] {
   x, y, z
};


namespace internal
{

virt_ptr<void>
getUserInterruptHandlerUserData(InterruptType interrupt,
                                RamProcessId ramPid);

} // namespace internal

void
KeDisableInterrupt(InterruptType interrupt)
{

}

void
KeEnableInterrupt(InterruptType interrupt)
{

}

void
KeClearAndEnableInterrupt(InterruptType interrupt)
{
   internal::kernelLockAcquire();
   internal::clearAndEnableInterrupt(interrupt, getRampid());
   internal::kernelLockRelease();
}

uint32_t
KeGetInterruptStatus(InterruptType interrupt)
{

}

uint32_t
KeClearInterruptStatus(InterruptType interrupt)
{

}

virt_ptr<void>
KeGetInterruptHandler(InterruptType interrupt)
{
   internal::kernelLockAcquire();
   auto result = internal::getUserInterruptHandlerUserData(interrupt,
                                                           getRampid());
   internal::kernelLockRelease();
   return result;
}

virt_ptr<void>
KeSetInterruptHandler(InterruptType interrupt,
                      UserInterruptHandler callback,
                      virt_ptr<void> callbackUserData,
                      virt_ptr<void> stack,
                      virt_ptr<Context> context)
{
   internal::kernelLockAcquire();
   auto result = internal::setUserInterruptHandler(interrupt,
                                                   callback,
                                                   callbackUserData,
                                                   stack,
                                                   context,
                                                   getRampid());
   internal::kernelLockRelease();
   return result;
}

namespace internal
{

struct InterruptHandler
{
   bool canModify = false;
   bool isRegistered = false;
   RamProcessId ramPid;
   KernelInterruptHandler kernelHandler;

   struct UserHandler
   {
      virt_ptr<void> userData;
      UserInterruptHandler callback;
      virt_ptr<void> stack;
      virt_ptr<Context> context;
   };

   UserHandler userHandler;
};

struct InterruptData
{
   std::array<InterruptHandler, static_cast<size_t>(InterruptType::Max)> handlers;
};

static std::array<InterruptData, 3>
sPerCoreInterruptData;

InterruptData &
getCoreInterruptData()
{
   return sPerCoreInterruptData[cpu::this_core::id()];
}

virt_ptr<void>
getUserInterruptHandlerUserData(InterruptType interrupt,
                                RamProcessId ramPid)
{
   auto &data = getCoreInterruptData();
   if (interrupt >= InterruptType::Max) {
      return 0;
   }

   auto &handler = data.handlers[static_cast<size_t>(interrupt)];
   if (!handler.isRegistered || handler.ramPid != ramPid) {
      return nullptr;
   }

   return handler.userHandler.userData;
}

virt_ptr<void>
setUserInterruptHandler(InterruptType interrupt,
                        UserInterruptHandler callback,
                        virt_ptr<void> callbackUserData,
                        virt_ptr<void> stack,
                        virt_ptr<Context> context,
                        RamProcessId ramPid)
{
   auto &data = getCoreInterruptData();
   if (interrupt >= InterruptType::Max) {
      return nullptr;
   }

   auto &handler = data.handlers[static_cast<size_t>(interrupt)];
   if (!handler.canModify) {
      // Registration disabled
      return nullptr;
   }

   if (handler.isRegistered && handler.ramPid != ramPid) {
      // Registered to someone else
      return nullptr;
   }

   auto previousUserData = virt_ptr<void> { nullptr };

   if (callbackUserData) {
      previousUserData = handler.userHandler.userData;
      handler.isRegistered = true;
      handler.userHandler.callback = callback;
      handler.userHandler.userData = callbackUserData;
      handler.userHandler.context = context;
      handler.userHandler.stack = stack;
   } else {
      previousUserData = handler.userHandler.userData;
      handler.isRegistered = false;
      std::memset(&handler.userHandler, 0, sizeof(InterruptHandler::UserHandler));
      handler.ramPid = RamProcessId::Invalid;
   }

   return previousUserData;
}

KernelInterruptHandler
setKernelInterruptHandler(InterruptType interrupt,
                          KernelInterruptHandler callback)
{
   auto &data = getCoreInterruptData();
   if (interrupt >= InterruptType::Max) {
      return nullptr;
   }

   auto &handler = data.handlers[static_cast<size_t>(interrupt)];
   if (!handler.canModify) {
      // Registration disabled
      return nullptr;
   }

   auto previous = handler.kernelHandler;
   handler.isRegistered = true;
   handler.ramPid = RamProcessId::Kernel;
   handler.kernelHandler = std::move(callback);
   std::memset(&handler.userHandler, 0, sizeof(InterruptHandler::UserHandler));
   return previous;

}

bool
clearAndEnableInterrupt(InterruptType interrupt,
                        RamProcessId ramPid)
{
   auto &data = getCoreInterruptData();
   if (interrupt >= InterruptType::Max) {
      return false;
   }

   auto &handler = data.handlers[static_cast<size_t>(interrupt)];
   if (!handler.canModify) {
      return false;
   }

   if (!handler.isRegistered || handler.ramPid != ramPid) {
      // Not registered to us
      return nullptr;
   }


}

static void
handleExternalInterruptException(ExceptionType type,
                                 virt_ptr<Context> interruptedContext)
{

}

void
initialiseInterruptHandlers()
{
   setKernelExceptionHandler(ExceptionType::ExternalInterrupt,
                             handleExternalInterruptException);
}

} // namespace internal

} // namespace cafe::kernel

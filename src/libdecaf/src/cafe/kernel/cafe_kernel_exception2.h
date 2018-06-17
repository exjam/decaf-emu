#pragma once
#include "cafe_kernel_context.h"
#include "cafe_kernel_process.h"
#include <libcpu/be2_struct.h>
#include <libcpu/cpu.h>

namespace cafe::kernel
{

enum class ExceptionType
{
   SystemReset          = 0,
   MachineCheck         = 1,
   DSI                  = 2,
   ISI                  = 3,
   ExternalInterrupt    = 4,
   Alignment            = 5,
   Program              = 6,
   FloatingPoint        = 7,
   Decrementer          = 8,
   SystemCall           = 9,
   Trace                = 10,
   PerformanceMonitor   = 11,
   Breakpoint           = 12,
   SystemInterrupt      = 13,
   ICI                  = 14,
   Max,
};

// __KernelSetUserModeExHandler

using UserModeExceptionCallbackFn = virt_func_ptr<
   void (ExceptionType type,
         virt_ptr<Context> interruptedContext,
         virt_ptr<Context> currentContext)>;

struct UserModeExceptionHandler
{
   be2_val<UserModeExceptionCallbackFn> callback;
   be2_virt_ptr<void> stack;
   be2_virt_ptr<Context> context;
};
CHECK_OFFSET(UserModeExceptionHandler, 0x00, callback);
CHECK_OFFSET(UserModeExceptionHandler, 0x04, stack);
CHECK_OFFSET(UserModeExceptionHandler, 0x08, context);
CHECK_SIZE(UserModeExceptionHandler, 0x0C);

void
KeSetUserModeExceptionHandler(ExceptionType type,
                              virt_ptr<UserModeExceptionHandler> handler,
                              virt_ptr<UserModeExceptionHandler> outChainInfo,
                              BOOL unk_r6);

namespace internal
{

using KernelExceptionHandler = std::function<
   void(ExceptionType type,
        virt_ptr<Context> interruptedContext)>;

void
initialiseExceptionHandlers();

void
setKernelExceptionHandler(ExceptionType type,
                          KernelExceptionHandler handler);

} // namespace internal

} // namespace cafe::kernel

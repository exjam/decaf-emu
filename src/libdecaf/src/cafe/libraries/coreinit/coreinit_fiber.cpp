#include "coreinit.h"
#include "coreinit_fiber.h"
#include "coreinit_thread.h"
#include "cafe/cafe_ppc_interface_invoke.h"
#include <libcpu/cpu.h>

namespace cafe::coreinit
{

/**
 * Switch to fiber.
 */
uint32_t
OSSwitchFiber(OSFiberEntryFn entry,
              uint32_t userStack)
{

   auto state = cpu::this_core::state();
   auto oldStack = state->gpr[1];

   // Set new stack
   internal::setUserStackPointer(userStack);
   state->gpr[1] = userStack;

   // Call fiber function
   auto result = cafe::invoke(cpu::this_core::state(), entry);

   // Restore old stack
   state->gpr[1] = oldStack;
   internal::removeUserStackPointer(oldStack);

   return result;
}

/**
 * Switch to fiber with 4 arguments.
 */
uint32_t
OSSwitchFiberEx(uint32_t arg1,
                uint32_t arg2,
                uint32_t arg3,
                uint32_t arg4,
                OSFiberExEntryFn entry,
                uint32_t userStack)
{
   auto state = cpu::this_core::state();
   auto oldStack = state->gpr[1];

   // Set new stack
   internal::setUserStackPointer(userStack);
   state->gpr[1] = userStack;

   // Call fiber function
   auto result = cafe::invoke(cpu::this_core::state(), entry,
                              arg1, arg2, arg3, arg4);

   // Restore old stack
   state->gpr[1] = oldStack;
   internal::removeUserStackPointer(oldStack);

   return result;
}

void
Library::registerFiberExports()
{
   RegisterFunctionExport(OSSwitchFiber);
   RegisterFunctionExport(OSSwitchFiberEx);
}

} // namespace cafe::coreinit

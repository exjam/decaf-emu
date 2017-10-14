#include "cafe_kernel_context.h"
#include "cafe/cafe_ppc_interface_invoke.h"

#include <array>
#include <common/platform_fiber.h>
#include <libcpu/cpu.h>

namespace cafe::kernel
{

struct HostContext
{
   platform::Fiber *fiber = nullptr;
   Context *context = nullptr;
   cpu::Tracer *tracer = nullptr;
};

static std::array<Context *, 3>
sCurrentContext;

static std::array<Context *, 3>
sDeadContext;

static std::array<Context, 3>
sIdleContext;

static std::array<platform::Fiber *, 3>
sIdleFiber;

static void
checkDeadContext();

using ContextEntryPoint = virt_func_ptr<void>;

void
saveContext(Context *context)
{
   auto state = cpu::this_core::state();

   for (auto i = 0; i < 32; ++i) {
      context->gpr[i] = state->gpr[i];
   }

   for (auto i = 0; i < 32; ++i) {
      context->fpr[i] = state->fpr[i].value;
      context->psf[i] = state->fpr[i].paired1;
   }

   for (auto i = 0; i < 8; ++i) {
      context->gqr[i] = state->gqr[i].value;
   }

   context->cr = state->cr.value;
   context->lr = state->lr;
   context->ctr = state->ctr;
   context->xer = state->xer.value;
   //context->srr0 = state->sr[0];
   //context->srr1 = state->sr[1];
   context->fpscr = state->fpscr.value;
}

void
restoreContext(Context *context)
{
   auto state = cpu::this_core::state();

   for (auto i = 0; i < 32; ++i) {
      state->gpr[i] = context->gpr[i];
   }

   for (auto i = 0; i < 32; ++i) {
      state->fpr[i].value = context->fpr[i];
      state->fpr[i].paired1 = context->psf[i];
   }

   for (auto i = 0; i < 8; ++i) {
      state->gqr[i].value = context->gqr[i];
   }

   state->cr.value = context->cr;
   state->lr = context->lr;
   state->ctr = context->ctr;
   state->xer.value = context->xer;
   //state->sr[0] = context->srr0;
   //state->sr[1] = context->srr1;
   state->fpscr.value = context->fpscr;
}

static void
sleepCurrentContext()
{
   // Grab the current core and context information
   auto core = cpu::this_core::state();
   auto context = sCurrentContext[core->id];

   if (context) {
      // Save all our registers to the context
      saveContext(context);
      context->nia = core->nia;
      context->cia = core->cia;
   } else {
      // We save the idle context's register information as well
      //  mainly so that it doesn't complain about core state loss.
      saveContext(&sIdleContext[core->id]);
   }

   // Some things to help us when debugging...
   core->nia = 0xFFFFFFFF;
   core->cia = 0xFFFFFFFF;
   cpu::this_core::setTracer(nullptr);
}

static void
wakeCurrentContext()
{
   // Clean up any dead fibers
   checkDeadContext();

   // Grab the current core and context information
   auto core = cpu::this_core::state();
   auto context = sCurrentContext[core->id];

   // If we switched into a new context, we need to restore it back
   //  to how it was configured before we suspended it.
   if (context) {
      // Restore our context from the OSContext
      restoreContext(context);
      core->nia = context->nia;
      core->cia = context->cia;

      // Some things to help us when debugging...
      cpu::this_core::setTracer(context->hostContext->tracer);
   } else {
      // Restore the idle context information stored earlier
      restoreContext(&sIdleContext[core->id]);

      // These are the 'defacto' idle-thread values
      core->nia = 0xFFFFFFFFu;
      core->cia = 0xFFFFFFFFu;
   }
}

static void
fiberEntryPoint(void*)
{
   /*
   THIS MUST BE MOVED TO coreinit::__crt_thread_entrypoint

   // The following code must follow the conventions set by reschedule/
   //  InitialiseThreadState as this method is technically emulating
   //  a PPC method (__crt_init).  It is worth mentioning that this
   //  fiberEntryPoint method essentially assumes that we are always
   //  called only when a thread is first started.  If this assumption
   //  ever needs to be broken, we will need to update InitThreadState
   //   so it calls an internal PPC function which does the behaviour.

   // We grab these before invoking the exception handler setup
   //  since it is plausible that it might damage the GPR's.
   auto core = cpu::this_core::state();
   auto entryPoint = coreinit::OSThreadEntryPointFn(core->nia);
   auto argc = core->gpr[3];
   auto argv = mem::translate<void>(core->gpr[4]);

   // Entrypoint is actually supposed to be adjusted to the __crt_init
   //  when you call OSCreateThread rather than having it called always
   //  but it doesn't hurt to have this set up on default threads.
   coreinit::internal::ghsInitExceptions();

   auto thread = coreinit::internal::getCurrentThread();
   gLog->info("Thread Starting: ptr {:08X}, id {:x} entry {:08X}",
      mem::untranslate(thread), thread->id, entryPoint.getAddress());

   auto exitValue = entryPoint(argc, argv);

   gLog->info("Thread Exiting: ptr {:08X}, id {:x}, exitValue {}",
      mem::untranslate(thread), thread->id, exitValue);

   coreinit::OSExitThread(exitValue);
   */

   // Load up the context set up by the InitialiseThreadEntry method.
   wakeCurrentContext();

   // Invoke the PPC thread entry point, note we do not pass any arguments
   // because whoever created the thread would have already put the arguments
   // into the registers.
   auto core = cpu::this_core::state();
   auto entryPoint = ContextEntryPoint { nullptr }; // virt_func_cast<ContextEntryPoint>(virt_addr { core->nia });
   invoke(core, entryPoint);
   decaf_check("Control flow returned to fiber entry point");
}

static HostContext *
allocateHostContext(Context *context)
{
   auto hostContext = new HostContext();
   hostContext->tracer = cpu::allocTracer(1024 * 10 * 10);
   hostContext->fiber = platform::createFiber(fiberEntryPoint, nullptr);
   hostContext->context = context;
   return hostContext;
}

void
reallocateContextFiber(Context *context,
                       platform::FiberEntryPoint entry)
{
   auto oldFiber = context->hostContext->fiber;
   auto newFiber = platform::createFiber(entry, nullptr);
   context->hostContext->fiber = newFiber;
   platform::swapToFiber(oldFiber, newFiber);
}

static void
freeHostContext(HostContext *hostContext)
{
   cpu::freeTracer(hostContext->tracer);
   platform::destroyFiber(hostContext->fiber);
   delete hostContext;
}

// This must be called under the same scheduler lock
// that added the thread to tDeadThread, we simply use
// the thread_local to pass it between fibers.
static void
checkDeadContext()
{
   auto coreId = cpu::this_core::id();
   auto deadContext = sDeadContext[coreId];

   if (deadContext) {
      sDeadContext[coreId] = nullptr;

      // Something broken if we are accidentally cleaning
      //  up currently active context...
      decaf_check(deadContext != sCurrentContext[coreId]);

      // Something is broken if we have no fiber
      decaf_check(deadContext->hostContext);

      // Destroy the fiber
      freeHostContext(deadContext->hostContext);
      deadContext->hostContext = nullptr;
   }
}

void
initCoreFiber()
{
   // Grab the currently running core state.
   auto coreId = cpu::this_core::id();

   // Grab the system fiber
   auto fiber = platform::getThreadFiber();

   // Save some needed information about the fiber run states.
   sIdleFiber[coreId] = fiber;
   sCurrentContext[coreId] = nullptr;
   sDeadContext[coreId] = nullptr;
}

void
exitThreadNoLock()
{
   auto coreId = cpu::this_core::id();

   // Make sure exitThread is not called multiple times
   decaf_check(!sDeadContext[coreId]);

   // Mark this fiber to be cleaned up
   sDeadContext[coreId] = sCurrentContext[coreId];
}

static platform::Fiber *
getContextFiber(Context *context)
{
   auto coreId = cpu::this_core::id();
   if (!context) {
      return sIdleFiber[coreId];
   }

   if (!context->hostContext) {
      context->hostContext = allocateHostContext(context);
   }

   return context->hostContext->fiber;
}

void
setContext(Context *next)
{
   // Don't do anything if we are switching to the same context.
   auto coreId = cpu::this_core::id();
   auto current = sCurrentContext[coreId];
   if (current == next) {
      return;
   }

   // Perform savage operations before the switch
   sleepCurrentContext();

   // Switch to the new fiber, note that coreId is no longer valid
   // after this point, as this context may have been switched to
   // a new core.
   sCurrentContext[coreId] = next;
   platform::swapToFiber(getContextFiber(current), getContextFiber(next));

   // Perform restoral operations after the switch
   wakeCurrentContext();
}


} // namespace cafe::kernel

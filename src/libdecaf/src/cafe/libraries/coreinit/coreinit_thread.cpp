#pragma optimize("", off)
#include "coreinit.h"
#include "coreinit_alarm.h"
#include "coreinit_core.h"
#include "coreinit_dynload.h"
#include "coreinit_enum_string.h"
#include "coreinit_fastmutex.h"
#include "coreinit_ghs.h"
#include "coreinit_interrupts.h"
#include "coreinit_memheap.h"
#include "coreinit_memory.h"
#include "coreinit_mutex.h"
#include "coreinit_scheduler.h"
#include "coreinit_systeminfo.h"
#include "coreinit_thread.h"

#include "cafe/cafe_stackobject.h"

#include <array>
#include <common/decaf_assert.h>
#include <common/log.h>
#include <fmt/format.h>
#include <libcpu/cpu.h>
#include <limits>

namespace cafe::coreinit
{

const uint32_t
OSThread::Tag;

static std::array<virt_ptr<OSThread> , CoreCount>
sDefaultThreads = { nullptr, nullptr, nullptr };

static uint16_t
sThreadId = 1;

static AlarmCallbackFn
sSleepAlarmHandler = nullptr;

static OSThreadEntryPointFn
sThreadEntryPoint = nullptr;

static OSThreadEntryPointFn
sDeallocatorThreadEntryPoint = nullptr;

constexpr auto DeallocatorThreadSize = 0x2000u;

struct StaticThreadData
{
   struct PerCoreData
   {
      be2_struct<OSThreadQueue> deallocationQueue;
      be2_struct<OSThreadQueue> deallocationThreadQueue;
      be2_struct<OSThread> deallocatorThread;
      be2_array<uint8_t, DeallocatorThreadSize> deallocatorThreadStack;
      be2_array<char, 40> deallocatorThreadName;
   };

   be2_array<PerCoreData, OSGetCoreCount()> perCoreData;
};

static virt_ptr<StaticThreadData>
sThreadData = nullptr;

void
__OSClearThreadStack32(virt_ptr<OSThread> thread,
                       uint32_t value)
{
   auto clearStart = virt_ptr<uint32_t> { nullptr };
   auto clearEnd = virt_ptr<uint32_t> { nullptr };

   if (OSGetCurrentThread() == thread) {
      clearStart = thread->stackEnd + 4;
      clearEnd = virt_cast<uint32_t *>(OSGetStackPointer());
   } else {
      // We assume that the thread must be paused while this is happening...
      // This might be a bad assumption to make, but otherwise we run
      // into some sketchy race conditions.
      clearStart = thread->stackEnd + 4;
      clearEnd = virt_cast<uint32_t *>(virt_addr { thread->context.gpr[1].value() });
   }

   for (auto addr = clearStart; addr < clearEnd; addr += 4) {
      *addr = value;
   }
}


/**
 * Cancels a thread.
 *
 * This sets the threads requestFlag to OSThreadRequest::Cancel, the thread will
 * be terminated next time OSTestThreadCancel is called.
 */
void
OSCancelThread(virt_ptr<OSThread> thread)
{
   bool reschedule = false;
   internal::lockScheduler();

   if (thread->requestFlag == OSThreadRequest::Suspend) {
      internal::wakeupThreadWaitForSuspensionNoLock(
         virt_addrof(thread->suspendQueue),
         -1);
      reschedule = true;
   }

   if (thread->suspendCounter != 0) {
      if (thread->cancelState == OSThreadCancelState::Enabled) {
         internal::resumeThreadNoLock(thread, thread->suspendCounter);
         reschedule = true;
      }
   }

   if (reschedule) {
      internal::rescheduleAllCoreNoLock();
   }

   thread->suspendCounter = 0;
   thread->needSuspend = 0;
   thread->requestFlag = OSThreadRequest::Cancel;
   internal::unlockScheduler();

   if (OSGetCurrentThread() == thread) {
      if (thread->cancelState == OSThreadCancelState::Enabled) {
         OSExitThread(-1);
      }
   }
}


/**
 * Returns the count of active threads.
 */
int32_t
OSCheckActiveThreads()
{
   internal::lockScheduler();
   virt_ptr<OSThread> thread = OSGetCurrentThread();

   auto threadCount = internal::checkActiveThreadsNoLock();

   internal::unlockScheduler();
   return threadCount;
}


/**
 * Get the maximum amount of stack the thread has used.
 */
int32_t
OSCheckThreadStackUsage(virt_ptr<OSThread> thread)
{
   auto addr = virt_ptr<uint32_t> { nullptr };
   internal::lockScheduler();

   for (addr = thread->stackEnd + 4; addr < thread->stackStart; addr += 4) {
      if (*addr != 0xFEFEFEFE) {
         break;
      }
   }

   auto result = virt_cast<virt_addr>(thread->stackStart) - virt_cast<virt_addr>(addr);
   internal::unlockScheduler();
   return static_cast<int32_t>(result);
}


/**
 * Disable tracking of thread stack usage
 */
void
OSClearThreadStackUsage(virt_ptr<OSThread> thread)
{
   internal::lockScheduler();

   if (!thread) {
      thread = OSGetCurrentThread();
   }

   thread->attr &= ~OSThreadAttributes::StackUsage;
   internal::unlockScheduler();
}


/**
 * Clears a thread's suspend counter and resumes it.
 */
void
OSContinueThread(virt_ptr<OSThread> thread)
{
   internal::lockScheduler();
   internal::resumeThreadNoLock(thread, thread->suspendCounter);
   internal::rescheduleAllCoreNoLock();
   internal::unlockScheduler();
}


/**
 * Thread entry.
 */
static uint32_t
threadEntry(uint32_t argc,
            virt_ptr<void> argv)
{
   auto thread = OSGetCurrentThread();
   auto interruptsState = OSDisableInterrupts();
   internal::ghsExceptionInit(thread);
   OSRestoreInterrupts(interruptsState);
   return cafe::invoke(cpu::this_core::state(),
                       thread->entryPoint,
                       argc,
                       argv);
}


/**
 * Setup thread run state, shared by OSRunThread and OSCreateThread
 */
static void
initialiseThreadState(virt_ptr<OSThread> thread,
                      OSThreadEntryPointFn entry,
                      uint32_t argc,
                      virt_ptr<void> argv)
{
   // TODO: Fix sdaBase, sda2Base, lr
   //auto module = kernel::getUserModule();
   auto sdaBase = 0u;//module ? module->sdaBase : 0u;
   auto sda2Base = 0u;// module ? module->sda2Base : 0u;

   decaf_check(thread->context.hostContext == nullptr);

   // Setup context
   thread->context.gpr[0] = 0u;
   thread->context.gpr[1] = static_cast<uint32_t>(virt_cast<virt_addr>(thread->stackStart)) - 8;
   thread->context.gpr[2] = sda2Base;
   thread->context.gpr[3] = argc;
   thread->context.gpr[4] = static_cast<uint32_t>(virt_cast<virt_addr>(argv));
   thread->context.gpr[13] = sdaBase;
   thread->context.fpscr = 4u;
   thread->context.gqr[2] = 0x40004u;
   thread->context.gqr[3] = 0x50005u;
   thread->context.gqr[4] = 0x60006u;
   thread->context.gqr[5] = 0x70007u;
   thread->context.lr = 0xDEADC0DE;//kernel::loader::findModule("coreinit")->findExport("OSExitThread");

   // Clear the backchain to 0
   auto stackPtr = mem::translate<uint32_t>(thread->context.gpr[1]);
   stackPtr[0] = 0;
   stackPtr[1] = 0;

   // Setup context Decaf special stuff
   thread->context.nia = virt_func_cast<virt_addr>(sThreadEntryPoint);
   thread->context.cia = 0xFFFFFFFF;

   // Setup thread
   thread->entryPoint = entry;
   thread->state = entry ? OSThreadState::Ready : OSThreadState::None;
   thread->requestFlag = OSThreadRequest::None;
   thread->suspendCounter = 1;
   thread->needSuspend = 0;
}


/**
 * Create a new thread.
 *
 * \param thread Thread to initialise.
 * \param entry Thread entry point.
 * \param argc argc argument passed to entry point.
 * \param argv argv argument passed to entry point.
 * \param stack Top of stack (highest address).
 * \param stackSize Size of stack.
 * \param priority Thread priority, 0 is highest priorty, 31 is lowest.
 * \param attributes Thread attributes, see OSThreadAttributes.
 */
BOOL
OSCreateThread(virt_ptr<OSThread> thread,
               OSThreadEntryPointFn entry,
               uint32_t argc,
               virt_ptr<void> argv,
               virt_ptr<uint32_t> stack,
               uint32_t stackSize,
               int32_t priority,
               OSThreadAttributes attributes)
{
   // If no affinity is defined, we need to copy the affinity from the calling thread
   if ((attributes & OSThreadAttributes::AffinityAny) == 0) {
      auto curAttr = internal::getCurrentThread()->attr;
      attributes = attributes | (curAttr & OSThreadAttributes::AffinityAny);
   }

   // Setup OSThread
   memset(thread, 0, sizeof(OSThread));
   thread->tag = OSThread::Tag;
   thread->stackStart = stack;
   thread->stackEnd = virt_cast<uint32_t *>(virt_cast<virt_addr >(stack) - stackSize);
   thread->userStackPointer = thread->stackStart;
   thread->basePriority = priority;
   thread->priority = thread->basePriority;
   thread->attr = attributes;

   // Write magic stack ending!
   *thread->stackEnd = 0xDEADBABE;

   // Setup thread state
   internal::lockScheduler();
   initialiseThreadState(thread, entry, argc, argv);
   thread->id = sThreadId++;

   if (entry) {
      internal::markThreadActiveNoLock(thread);
   }

   internal::unlockScheduler();

   gLog->info("Thread Created: ptr 0x{:08X}, id 0x{:X}, basePriority {}, attr 0x{:02X}, entry 0x{:08X}, stackStart 0x{:08X}, stackEnd 0x{:08X}",
      thread, thread->id, thread->basePriority, thread->attr,
      virt_func_cast<virt_addr>(entry), thread->stackStart, thread->stackEnd);

   return TRUE;
}


/**
 * Detach thread.
 */
void
OSDetachThread(virt_ptr<OSThread> thread)
{
   internal::lockScheduler();

   // HACK: Unfortunately this check is not valid in all games.  One Piece performs
   //  OSJoinThread on a thread, and then subsequently calls OSDetachThread on it
   //  for whatever reason.  Coreinit doesnt check this, so we can't do this check.
   //decaf_check(internal::isThreadActiveNoLock(thread));

   thread->attr |= OSThreadAttributes::Detached;

   if (thread->state == OSThreadState::Moribund) {
      // Thread has already ended so we can remove it from the active list
      internal::markThreadInactiveNoLock(thread);

      if (thread->deallocator) {
         internal::queueThreadDeallocation(thread);
      }

      thread->state = OSThreadState::None;
      // TODO: thread->id = 0x8000;
   }

   internal::wakeupThreadNoLock(virt_addrof(thread->joinQueue));
   internal::rescheduleAllCoreNoLock();
   internal::unlockScheduler();
}


/**
 * Exit the current thread with a exit code.
 *
 * This function is implicitly called when the thread entry point returns.
 */
void
OSExitThread(int value)
{
   auto thread = OSGetCurrentThread();

   // Call any thread cleanup callbacks
   if (thread->cleanupCallback) {
      thread->cancelState |= OSThreadCancelState::Disabled;
      cafe::invoke(cpu::this_core::state(),
                   thread->cleanupCallback,
                   thread,
                   virt_cast<void *>(thread->stackEnd));
   }

   // Cleanup the GHS exceptions we previously created
   internal::ghsExceptionCleanup(thread);

   // Free any TLS data which was allocated to this thread
   if (thread->tlsSections) {
      internal::dynLoadTlsFree(thread);
   }

   // Disable interrupts and lock the scheduler
   auto oldInterrupts = OSDisableInterrupts();
   internal::lockScheduler();

   // Actually proccess the thread exit
   internal::exitThreadNoLock(value);

   // We should never reach here.  The scheduler handles unlocking
   //  itself and restoring the interrupt state.
   decaf_abort("exitThreadNoLock returned");
}


/**
 * Get the next and previous thread in the thread's active queue.
 */
void
OSGetActiveThreadLink(virt_ptr<OSThread> thread,
                      virt_ptr<OSThreadLink> link)
{
   *link = thread->activeLink;
}


/**
 * Return pointer to OSThread object for the current thread.
 */
virt_ptr<OSThread>
OSGetCurrentThread()
{
   return internal::getCurrentThread();
}


/**
 * Returns the default thread for a specific core.
 *
 * Each core has 1 default thread created before the game boots. The default
 * thread for core 1 calls the RPX entry point, the default threads for core 0
 * and 2 are suspended and can be used with OSRunThread.
 */
virt_ptr<OSThread>
OSGetDefaultThread(uint32_t coreID)
{
   if (coreID >= CoreCount) {
      return nullptr;
   }

   return sDefaultThreads[coreID];
}


/**
 * Return current stack pointer, value of r1 register.
 */
virt_ptr<uint32_t>
OSGetStackPointer()
{
   return virt_cast<uint32_t *>(virt_addr { cpu::this_core::state()->gpr[1] });
}


/**
 * Return user stack pointer.
 */
virt_ptr<uint32_t>
OSGetUserStackPointer(virt_ptr<OSThread> thread)
{
   auto stack = virt_ptr<uint32_t> { nullptr };
   internal::lockScheduler();

   if (OSIsThreadSuspended(thread)) {
      stack = thread->userStackPointer;

      if (!stack) {
         stack = virt_cast<uint32_t *>(virt_addr { thread->context.gpr[1].value() });
      }
   }

   internal::unlockScheduler();
   return stack;
}


/**
 * Get a thread's affinity.
 */
uint32_t
OSGetThreadAffinity(virt_ptr<OSThread> thread)
{
   return thread->attr & OSThreadAttributes::AffinityAny;
}


/**
 * Get a thread's name.
 */
virt_ptr<const char>
OSGetThreadName(virt_ptr<OSThread> thread)
{
   return thread->name;
}


/**
 * Get a thread's base priority.
 */
int32_t
OSGetThreadPriority(virt_ptr<OSThread> thread)
{
   return thread->basePriority;
}


/**
 * Get a thread's specific value set by OSSetThreadSpecific.
 */
uint32_t
OSGetThreadSpecific(uint32_t id)
{
   decaf_check(id >= 0 && id < 0x10);
   return OSGetCurrentThread()->specific[id];
}


/**
* Initialise a thread queue object.
*/
void
OSInitThreadQueue(virt_ptr<OSThreadQueue> queue)
{
   OSInitThreadQueueEx(queue, nullptr);
}


/**
* Initialise a thread queue object with a parent.
*/
void
OSInitThreadQueueEx(virt_ptr<OSThreadQueue> queue,
                    virt_ptr<void> parent)
{
   queue->head = nullptr;
   queue->tail = nullptr;
   queue->parent = parent;
}


/**
 * Returns TRUE if a thread is suspended.
 */
BOOL
OSIsThreadSuspended(virt_ptr<OSThread> thread)
{
   return thread->suspendCounter > 0;
}


/**
 * Returns TRUE if a thread is terminated.
 */
BOOL
OSIsThreadTerminated(virt_ptr<OSThread> thread)
{
   return thread->state == OSThreadState::None
       || thread->state == OSThreadState::Moribund;
}


/**
 * Wait until thread is terminated.
 *
 * \param thread Thread to wait for
 * \param exitValue Pointer to store thread exit value in.
 * \returns Returns TRUE if thread has terminated, FALSE otherwise.
 */
BOOL
OSJoinThread(virt_ptr<OSThread> thread,
             virt_ptr<int32_t> outExitValue)
{
   internal::lockScheduler();
   decaf_check(thread);
   decaf_check(internal::isThreadActiveNoLock(thread));

   // If the thread has not ended, let's wait for it
   //  note only one thread is allowed in the join queue
   if (thread->state != OSThreadState::Moribund && !thread->joinQueue.head) {
      internal::sleepThreadNoLock(virt_addrof(thread->joinQueue));
      internal::rescheduleSelfNoLock();

      if (!internal::isThreadActiveNoLock(thread)) {
         // This would only happen for detached threads.
         internal::unlockScheduler();
         return FALSE;
      }
   }

   if (thread->state != OSThreadState::Moribund) {
      internal::unlockScheduler();
      return FALSE;
   }

   if (outExitValue) {
      *outExitValue = thread->exitValue;
   }

   internal::markThreadInactiveNoLock(thread);
   thread->state = OSThreadState::None;

   if (thread->deallocator) {
      internal::queueThreadDeallocation(thread);
      internal::rescheduleSelfNoLock();
   }

   internal::unlockScheduler();
   return TRUE;
}


void
OSPrintCurrentThreadState()
{
   auto thread = OSGetCurrentThread();

   if (!thread) {
      return;
   }

   auto state = cpu::this_core::state();

   fmt::MemoryWriter out;
   out.write("id   = {}\n", thread->id);

   if (thread->name) {
      out.write("name  = {}\n", thread->name);
   }

   out.write("cia   = 0x{:08X}\n", state->cia);
   out.write("lr    = 0x{:08X}\n", state->lr);
   out.write("cr    = 0x{:08X}\n", state->cr.value);
   out.write("xer   = 0x{:08X}\n", state->xer.value);
   out.write("ctr   = 0x{:08X}\n", state->ctr);

   for (auto i = 0u; i < 32; ++i) {
      out.write("r{:<2}   = 0x{:08X}\n", i, state->gpr[i]);
   }

   out.write("fpscr = 0x{:08X}\n", state->fpscr.value);

   for (auto i = 0u; i < 32; ++i) {
      out.write("f{:<2}   = {}\n", i, state->fpr[i].value);
   }

   for (auto i = 0u; i < 32; ++i) {
      out.write("ps{:<2}   = {:<16} ps{:<2}   = {}\n", i, state->fpr[i].paired0, i, state->fpr[i].paired1);
   }

   gLog->info(out.str());
}


/**
 * Resumes a thread.
 *
 * Decrements the thread's suspend counter, if the counter reaches 0 the thread
 * is resumed.
 *
 * \returns Returns the previous value of the suspend counter.
 */
int32_t
OSResumeThread(virt_ptr<OSThread> thread)
{
   internal::lockScheduler();
   auto oldSuspendCounter = internal::resumeThreadNoLock(thread, 1);

   if (oldSuspendCounter - 1 == 0) {
      internal::rescheduleAllCoreNoLock();
   }

   internal::unlockScheduler();
   return oldSuspendCounter;
}


/**
 * Run a function on an already created thread.
 *
 * Can only be used on idle threads.
 */
BOOL
OSRunThread(virt_ptr<OSThread> thread,
            OSThreadEntryPointFn entry,
            uint32_t argc,
            virt_ptr<void> argv)
{
   BOOL result = FALSE;
   internal::lockScheduler();

   if (OSIsThreadTerminated(thread)) {
      if (thread->state == OSThreadState::Moribund) {
         internal::markThreadInactiveNoLock(thread);
      }

      initialiseThreadState(thread, entry, argc, argv);
      internal::markThreadActiveNoLock(thread);
      internal::resumeThreadNoLock(thread, 1);
      internal::rescheduleAllCoreNoLock();
      result = TRUE;
   }

   internal::unlockScheduler();
   return result;
}


/**
 * Set a thread's affinity.
 */
BOOL
OSSetThreadAffinity(virt_ptr<OSThread> thread,
                    uint32_t affinity)
{
   internal::lockScheduler();
   internal::setThreadAffinityNoLock(thread, affinity);

   if (thread->state == OSThreadState::Ready && affinity != 0) {
      internal::rescheduleAllCoreNoLock();
   }

   internal::unlockScheduler();
   return TRUE;
}


/**
 * Set a thread's cancellation state.
 */
BOOL
OSSetThreadCancelState(BOOL cancelEnabled)
{
   auto thread = OSGetCurrentThread();
   auto oldCancelEnabled = TRUE;

   if (thread->cancelState & OSThreadCancelState::Disabled) {
      oldCancelEnabled = FALSE;
   }

   if (cancelEnabled) {
      thread->cancelState &= ~OSThreadCancelState::Disabled;
   } else {
      thread->cancelState |= OSThreadCancelState::Disabled;
   }

   return oldCancelEnabled;
}


/**
 * Set the callback to be called just before a thread is terminated.
 *
 * \return Returns the previous callback function.
 */
OSThreadCleanupCallbackFn
OSSetThreadCleanupCallback(virt_ptr<OSThread> thread,
                           OSThreadCleanupCallbackFn callback)
{
   internal::lockScheduler();
   auto old = thread->cleanupCallback;
   thread->cleanupCallback = callback;
   internal::unlockScheduler();
   return old;
}


/**
 * Set the callback to be called just after a thread is terminated.
 */
OSThreadDeallocatorFn
OSSetThreadDeallocator(virt_ptr<OSThread> thread,
                       OSThreadDeallocatorFn deallocator)
{
   internal::lockScheduler();
   auto old = thread->deallocator;
   thread->deallocator = deallocator;
   internal::unlockScheduler();
   return old;
}


/**
 * Set a thread's name.
 */
void
OSSetThreadName(virt_ptr<OSThread> thread,
                virt_ptr<const char> name)
{
   thread->name = name;
}


/**
 * Set a thread's priority.
 */
BOOL
OSSetThreadPriority(virt_ptr<OSThread> thread,
                    int32_t priority)
{
   if (priority > 31) {
      return FALSE;
   }

   internal::lockScheduler();
   thread->basePriority = priority;
   internal::updateThreadPriorityNoLock(thread);
   internal::rescheduleAllCoreNoLock();
   internal::unlockScheduler();
   return TRUE;
}


/**
 * Set a thread's run quantum.
 *
 * This is the maximum amount of time the thread can run for before being forced
 * to yield.
 */
BOOL
OSSetThreadRunQuantum(virt_ptr<OSThread> thread,
                      uint32_t quantumUS)
{
   if (quantumUS != OSThreadQuantum::Infinite) {
      if (quantumUS < OSThreadQuantum::MinMicroseconds) {
         return FALSE;
      }

      if (quantumUS > OSThreadQuantum::MaxMicroseconds) {
         return FALSE;
      }
   }

   auto ticks = internal::usToTicks(quantumUS);
   auto result = FALSE;

   internal::lockScheduler();
   result = internal::setThreadRunQuantumNoLock(thread, ticks);
   internal::unlockScheduler();
   return result;
}


/**
 * Set a thread specific value.
 *
 * Can be read with OSGetThreadSpecific.
 */
void
OSSetThreadSpecific(uint32_t id,
                    uint32_t value)
{
   OSGetCurrentThread()->specific[id] = value;
}


/**
 * Set thread stack usage tracking.
 */
BOOL
OSSetThreadStackUsage(virt_ptr<OSThread> thread)
{
   internal::lockScheduler();

   if (!thread) {
      thread = OSGetCurrentThread();
   } else if (thread->state == OSThreadState::Running) {
      internal::unlockScheduler();
      return FALSE;
   }

   __OSClearThreadStack32(thread, 0xfefefefe);
   thread->attr |= OSThreadAttributes::StackUsage;
   internal::unlockScheduler();
   return TRUE;
}


/**
 * Sleep the current thread and add it to a thread queue.
 *
 * Will sleep until the thread queue is woken with OSWakeupThread.
 */
void
OSSleepThread(virt_ptr<OSThreadQueue> queue)
{
   internal::lockScheduler();
   internal::sleepThreadNoLock(queue);
   internal::rescheduleSelfNoLock();
   internal::unlockScheduler();
}

static void
sleepAlarmHandler(virt_ptr<OSAlarm> alarm,
                  virt_ptr<OSContext> context)
{
   // Wakeup the thread waiting on this alarm
   auto data = virt_cast<OSThread *>(OSGetAlarmUserData(alarm));

   // System Alarm, we already have the scheduler lock
   internal::wakeupOneThreadNoLock(data);
}

/**
 * Sleep the current thread for a period of time.
 */
void
OSSleepTicks(OSTime ticks)
{
   // Create an alarm to trigger wakeup
   StackObject<OSAlarm> alarm;
   StackObject<OSThreadQueue> queue;

   OSCreateAlarm(alarm);
   OSInitThreadQueue(queue);

   internal::lockScheduler();
   internal::setAlarmInternal(alarm, ticks, sSleepAlarmHandler, OSGetCurrentThread());

   internal::sleepThreadNoLock(queue);
   internal::rescheduleSelfNoLock();

   internal::unlockScheduler();
}


/**
 * Suspend a thread.
 *
 * Increases a thread's suspend counter, if the counter is >0 then the thread is
 * suspended.
 *
 * \returns Returns the thread's previous suspend counter value
 */
uint32_t
OSSuspendThread(virt_ptr<OSThread> thread)
{
   internal::lockScheduler();
   int32_t result = -1;

   if (thread->state == OSThreadState::Moribund || thread->state == OSThreadState::None) {
      internal::unlockScheduler();
      return -1;
   }

   if (thread->requestFlag == OSThreadRequest::Cancel) {
      internal::unlockScheduler();
      return -1;
   }

   auto curThread = OSGetCurrentThread();

   if (curThread == thread) {
      if (thread->cancelState == OSThreadCancelState::Enabled) {
         thread->needSuspend++;
         result = thread->suspendCounter;
         internal::suspendThreadNoLock(thread);
         internal::rescheduleAllCoreNoLock();
      }
   } else {
      if (thread->suspendCounter != 0) {
         result = thread->suspendCounter++;
      } else {
         thread->needSuspend++;
         thread->requestFlag = OSThreadRequest::Suspend;
         internal::sleepThreadNoLock(virt_addrof(thread->suspendQueue));
         internal::rescheduleSelfNoLock();
         result = thread->suspendResult;
      }
   }

   internal::unlockScheduler();
   return result;
}


/**
 * Check to see if the current thread should be cancelled or suspended.
 *
 * This is implicitly called in:
 * - OSLockMutex
 * - OSTryLockMutex
 * - OSUnlockMutex
 * - OSAcquireSpinLock
 * - OSTryAcquireSpinLock
 * - OSTryAcquireSpinLockWithTimeout
 * - OSReleaseSpinLock
 * - OSCancelThread
 */
void
OSTestThreadCancel()
{
   internal::lockScheduler();
   internal::testThreadCancelNoLock();
   internal::unlockScheduler();
}


/**
 * Wake up all threads in queue.
 *
 * Clears the thread queue.
 */
void
OSWakeupThread(virt_ptr<OSThreadQueue> queue)
{
   internal::lockScheduler();
   internal::wakeupThreadNoLock(queue);
   internal::rescheduleAllCoreNoLock();
   internal::unlockScheduler();
}


/**
 * Yield execution to waiting threads with same priority.
 *
 * This will never switch to a thread with a lower priority than the current
 * thread.
 */
void
OSYieldThread()
{
   internal::lockScheduler();
   internal::checkRunningThreadNoLock(true);
   internal::unlockScheduler();
}

uint32_t
deallocatorThreadEntry(uint32_t coreId,
                       virt_ptr<void>)
{
   auto &perCoreData = sThreadData->perCoreData[cpu::this_core::id()];
   auto waitQueue = virt_addrof(perCoreData.deallocationThreadQueue);
   auto queue = virt_addrof(perCoreData.deallocationQueue);

   auto oldInterrupts = OSDisableInterrupts();

   while (true) {
      auto thread = internal::ThreadQueue::popFront(queue);

      if (!thread) {
         internal::lockScheduler();
         internal::sleepThreadNoLock(waitQueue);
         internal::rescheduleSelfNoLock();
         internal::unlockScheduler();
         continue;
      }

      if (thread->deallocator) {
         OSRestoreInterrupts(oldInterrupts);

         cafe::invoke(cpu::this_core::state(),
                      thread->deallocator,
                      thread,
                      virt_cast<void *>(thread->stackEnd));
         oldInterrupts = OSDisableInterrupts();
      }
   }

   OSRestoreInterrupts(oldInterrupts);
}

namespace internal
{

/**
 * Set a user stack pointer for the current thread.
 */
void
setUserStackPointer(virt_ptr<uint32_t> stack)
{
   auto thread = OSGetCurrentThread();

   if (stack >= thread->stackEnd && stack < thread->stackStart) {
      // Cannot modify stack to within current stack frame.
      return;
   }

   auto current = OSGetStackPointer();

   if (current < thread->stackEnd || current >= thread->stackStart) {
      // If current stack is outside stack frame, then we must already have
      // a user stack pointer, and we shouldn't overwrite it.
      return;
   }

   thread->userStackPointer = stack;
   OSTestThreadCancel();
   thread->cancelState |= OSThreadCancelState::DisabledByUserStackPointer;
}


/**
 * Remove the user stack pointer for the current thread.
 */
void
removeUserStackPointer(virt_ptr<uint32_t> stack)
{
   auto thread = OSGetCurrentThread();

   if (stack < thread->stackEnd || stack >= thread->stackStart) {
      // If restore stack pointer is outside stack frame, then it is not
      // really restoring the original stack.
      return;
   }

   thread->cancelState &= ~OSThreadCancelState::DisabledByUserStackPointer;
   thread->userStackPointer = nullptr;
   OSTestThreadCancel();
}


/**
 * Set the current thread to run only on the current core.
 *
 * \return
 * Returns the old thread affinity, to be restored with unpinThreadAffinity.
 */
uint32_t
pinThreadAffinity()
{
   auto core = OSGetCoreId();
   auto thread = OSGetCurrentThread();
   internal::lockScheduler();

   auto oldAffinity = thread->attr & OSThreadAttributes::AffinityAny;
   thread->attr &= ~OSThreadAttributes::AffinityAny;
   thread->attr |= 1 << core;

   internal::unlockScheduler();
   return oldAffinity;
}


/**
 * Restores the thread affinity.
 */
void
unpinThreadAffinity(uint32_t affinity)
{
   auto thread = OSGetCurrentThread();
   internal::lockScheduler();
   thread->attr &= ~OSThreadAttributes::AffinityAny;
   thread->attr |= affinity;
   internal::unlockScheduler();
}


void
exitThreadNoLock(int32_t value)
{
   auto thread = OSGetCurrentThread();

   decaf_check(thread->state == OSThreadState::Running);
   decaf_check(internal::isThreadActiveNoLock(thread));

   // Clear the context associated with this thread

   if (thread->attr & OSThreadAttributes::Detached) {
      internal::markThreadInactiveNoLock(thread);
      thread->state = OSThreadState::None;
      // TODO: thread->id = 0x8000;

      if (thread->deallocator) {
         queueThreadDeallocation(thread);
      }
   } else {
      thread->exitValue = value;
      thread->state = OSThreadState::Moribund;
   }

   internal::disableScheduler();
   internal::unlockAllMutexNoLock(thread);
   internal::unlockAllFastMutexNoLock(thread);
   internal::wakeupThreadNoLock(virt_addrof(thread->joinQueue));
   internal::wakeupThreadWaitForSuspensionNoLock(virt_addrof(thread->suspendQueue), -1);
   internal::rescheduleAllCoreNoLock();
   internal::enableScheduler();

   cafe::kernel::exitThreadNoLock();
   internal::rescheduleSelfNoLock();

   // We do not need to unlockScheduler as OSExitThread never returns.
   decaf_abort("Exited thread was rescheduled...");
}

void
queueThreadDeallocation(virt_ptr<OSThread> thread)
{
   auto &perCoreData = sThreadData->perCoreData[cpu::this_core::id()];
   ThreadQueue::insert(virt_addrof(perCoreData.deallocationQueue), thread);
   wakeupThreadNoLock(virt_addrof(perCoreData.deallocationThreadQueue));
}

void
startDeallocatorThreads()
{
   for (auto i = 0; i < CoreCount; ++i) {
      auto &perCoreData = sThreadData->perCoreData[i];
      OSInitThreadQueue(virt_addrof(perCoreData.deallocationThreadQueue));
      OSInitThreadQueue(virt_addrof(perCoreData.deallocationQueue));

      auto thread = virt_addrof(perCoreData.deallocatorThread);
      auto stack = virt_addrof(perCoreData.deallocatorThreadStack);
      auto stackSize = perCoreData.deallocatorThreadStack.size();
      perCoreData.deallocatorThreadName = fmt::format("{{SYS Thread Terminator Core {}}}", i);


      OSCreateThread(thread, sDeallocatorThreadEntryPoint, i, nullptr,
                     virt_cast<uint32_t *>(stack + stackSize),
                     stackSize,
                     -1,
                     static_cast<OSThreadAttributes>(1 << i));
      OSSetThreadName(thread, virt_addrof(perCoreData.deallocatorThreadName));
      OSResumeThread(thread);
   }
}

void
setDefaultThread(uint32_t core,
                 virt_ptr<OSThread> thread)
{
   sDefaultThreads[core] = thread;
}

} // namespace internal

void
Library::registerThreadSymbols()
{
   RegisterFunctionExport(OSCancelThread);
   RegisterFunctionExport(OSCheckActiveThreads);
   RegisterFunctionExport(OSCheckThreadStackUsage);
   RegisterFunctionExport(OSClearThreadStackUsage);
   RegisterFunctionExport(OSContinueThread);
   RegisterFunctionExport(OSCreateThread);
   RegisterFunctionExport(OSDetachThread);
   RegisterFunctionExport(OSExitThread);
   RegisterFunctionExport(OSGetActiveThreadLink);
   RegisterFunctionExport(OSGetCurrentThread);
   RegisterFunctionExport(OSGetDefaultThread);
   RegisterFunctionExport(OSGetStackPointer);
   RegisterFunctionExport(OSGetUserStackPointer);
   RegisterFunctionExport(OSGetThreadAffinity);
   RegisterFunctionExport(OSGetThreadName);
   RegisterFunctionExport(OSGetThreadPriority);
   RegisterFunctionExport(OSGetThreadSpecific);
   RegisterFunctionExport(OSInitThreadQueue);
   RegisterFunctionExport(OSInitThreadQueueEx);
   RegisterFunctionExport(OSIsThreadSuspended);
   RegisterFunctionExport(OSIsThreadTerminated);
   RegisterFunctionExport(OSJoinThread);
   RegisterFunctionExport(OSPrintCurrentThreadState);
   RegisterFunctionExport(OSResumeThread);
   RegisterFunctionExport(OSRunThread);
   RegisterFunctionExport(OSSetThreadAffinity);
   RegisterFunctionExport(OSSetThreadCancelState);
   RegisterFunctionExport(OSSetThreadCleanupCallback);
   RegisterFunctionExport(OSSetThreadDeallocator);
   RegisterFunctionExport(OSSetThreadName);
   RegisterFunctionExport(OSSetThreadPriority);
   RegisterFunctionExport(OSSetThreadRunQuantum);
   RegisterFunctionExport(OSSetThreadSpecific);
   RegisterFunctionExport(OSSetThreadStackUsage);
   RegisterFunctionExport(OSSleepThread);
   RegisterFunctionExport(OSSleepTicks);
   RegisterFunctionExport(OSSuspendThread);
   RegisterFunctionExport(OSTestThreadCancel);
   RegisterFunctionExport(OSWakeupThread);
   RegisterFunctionExport(OSYieldThread);

   RegisterDataInternal(sThreadData);
   RegisterFunctionInternal(threadEntry, sThreadEntryPoint);
   RegisterFunctionInternal(sleepAlarmHandler, sSleepAlarmHandler);
   RegisterFunctionInternal(deallocatorThreadEntry, sDeallocatorThreadEntryPoint);
}

} // namespace Internalcafe::coreinit

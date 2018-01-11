#include "coreinit.h"
#include "coreinit_alarm.h"
#include "coreinit_core.h"
#include "coreinit_spinlock.h"
#include "coreinit_interrupts.h"
#include "coreinit_scheduler.h"
#include "coreinit_thread.h"
#include "coreinit_memheap.h"
#include "coreinit_memory.h"
#include "coreinit_time.h"
#include "coreinit_internal_queue.h"
#include "coreinit_internal_idlock.h"

#include <array>
#include <common/decaf_assert.h>
#include <fmt/format.h>

namespace cafe::coreinit
{

constexpr auto AlarmThreadStackSize = 0x8000u;

struct StaticAlarmData
{
   be2_val<OSThreadEntryPointFn> alarmCallbackThreadEntryPoint;
   internal::IdLock lock;

   struct PerCoreAlarmData
   {
      be2_struct<OSThread> thread;
      be2_array<char, 16> threadName;
      be2_array<uint8_t, AlarmThreadStackSize> threadStack;
      be2_struct<OSAlarmQueue> alarmQueue;
      be2_struct<OSAlarmQueue> callbackAlarmQueue;
      be2_struct<OSThreadQueue> callbackThreadQueue;
   };

   be2_array<PerCoreAlarmData, OSGetCoreCount()> perCoreData;
};

static virt_ptr<StaticAlarmData>
getAlarmData()
{
   return Library::getStaticData()->alarmData;
}

namespace internal
{

using AlarmQueue = Queue<OSAlarmQueue, OSAlarmLink, OSAlarm, &OSAlarm::link>;
void updateCpuAlarmNoALock();

} // namespace internal

/**
 * Internal alarm cancel.
 *
 * Reset the alarm state to cancelled.
 * Wakes up all threads waiting on the alarm.
 * Removes the alarm from any queue it is in.
 */
static BOOL
cancelAlarmNoAlarmLock(virt_ptr<OSAlarm> alarm)
{
   // We are technically supposed to try and remove the alarm
   //  from the callback queue as well.
   if (alarm->state != OSAlarmState::Set) {
      return FALSE;
   }

   alarm->state = OSAlarmState::Idle;
   alarm->nextFire = 0;
   alarm->period = 0;

   if (alarm->alarmQueue) {
      internal::AlarmQueue::erase(alarm->alarmQueue, alarm);
      alarm->alarmQueue = nullptr;
   }

   return TRUE;
}


/**
 * Cancel an alarm.
 */
BOOL
OSCancelAlarm(virt_ptr<OSAlarm> alarm)
{
   auto alarmData = getAlarmData();

   // First cancel the alarm whilst holding alarm lock
   internal::acquireIdLock(alarmData->lock, alarm);
   auto result = cancelAlarmNoAlarmLock(alarm);
   internal::releaseIdLock(alarmData->lock, alarm);

   if (!result) {
      return FALSE;
   }

   // Now wakeup any waiting threads
   internal::lockScheduler();

   for (auto thread = alarm->threadQueue.head; thread; thread = thread->link.next) {
      thread->alarmCancelled = TRUE;
   }

   internal::wakeupThreadNoLock(virt_addrof(alarm->threadQueue));
   internal::rescheduleAllCoreNoLock();
   internal::unlockScheduler();
   return TRUE;
}


/**
 * Cancel all alarms which have a matching tag.
 */
void
OSCancelAlarms(uint32_t group)
{
   auto alarmData = getAlarmData();
   internal::lockScheduler();
   internal::acquireIdLock(alarmData->lock, -2);

   for (auto &perCoreData : alarmData->perCoreData) {
      for (auto alarm = perCoreData.alarmQueue.head; alarm; ) {
         auto next = alarm->link.next;

         if (alarm->group == group) {
            if (cancelAlarmNoAlarmLock(alarm)) {
               for (auto thread = alarm->threadQueue.head; thread; thread = thread->link.next) {
                  thread->alarmCancelled = TRUE;
               }

               internal::wakeupThreadNoLock(virt_addrof(alarm->threadQueue));
            }
         }

         alarm = next;
      }
   }

   internal::releaseIdLock(alarmData->lock, -2);
   internal::rescheduleAllCoreNoLock();
   internal::unlockScheduler();
}


/**
 * Initialise an alarm structure.
 */
void
OSCreateAlarm(virt_ptr<OSAlarm> alarm)
{
   OSCreateAlarmEx(alarm, nullptr);
}


/**
 * Initialise an alarm structure.
 */
void
OSCreateAlarmEx(virt_ptr<OSAlarm> alarm,
                virt_ptr<const char> name)
{
   auto alarmData = getAlarmData();
   // Holding the alarm here is neccessary since its valid to call
   // Create on an already active alarm, as long as its not set.
   internal::acquireIdLock(alarmData->lock, alarm);

   memset(alarm, 0, sizeof(OSAlarm));
   alarm->tag = OSAlarm::Tag;
   alarm->name = name;
   OSInitThreadQueueEx(virt_addrof(alarm->threadQueue), alarm);

   internal::releaseIdLock(alarmData->lock, alarm);
}


/**
 * Return the user data stored in the alarm using OSSetAlarmUserData
 */
virt_ptr<void>
OSGetAlarmUserData(virt_ptr<OSAlarm> alarm)
{
   return alarm->userData;
}


/**
 * Initialise an alarm queue structure
 */
void
OSInitAlarmQueue(virt_ptr<OSAlarmQueue> queue)
{
   OSInitAlarmQueueEx(queue, nullptr);
}


/**
 * Initialise an alarm queue structure with a name
 */
void
OSInitAlarmQueueEx(virt_ptr<OSAlarmQueue> queue,
                   virt_ptr<const char> name)
{
   memset(queue, 0, sizeof(OSAlarmQueue));
   queue->tag = OSAlarmQueue::Tag;
   queue->name = name;
}


/**
 * Set a one shot alarm to perform a callback after an amount of time.
 */
BOOL
OSSetAlarm(virt_ptr<OSAlarm> alarm,
           OSTime time,
           AlarmCallbackFn callback)
{
   return OSSetPeriodicAlarm(alarm, OSGetTime() + time, 0, callback);
}


/**
 * Set a repeated alarm to execute a callback every interval from start.
 *
 * \param alarm
 * The alarm to set.
 *
 * \param start
 * The duration until the alarm should first be triggered.
 *
 * \param interval
 * The interval between triggers after the first trigger.
 *
 * \param callback
 * The alarm callback to call when the alarm is triggered.
 *
 * \return
 * Returns TRUE if the alarm was succesfully set, FALSE otherwise.
 */
BOOL
OSSetPeriodicAlarm(virt_ptr<OSAlarm> alarm,
                   OSTime start,
                   OSTime interval,
                   AlarmCallbackFn callback)
{
   auto alarmData = getAlarmData();
   internal::acquireIdLock(alarmData->lock, alarm);

   // Set alarm
   alarm->nextFire = start;
   alarm->callback = callback;
   alarm->period = interval;
   alarm->context = nullptr;
   alarm->state = OSAlarmState::Set;

   // Erase from old alarm queue
   if (alarm->alarmQueue) {
      internal::AlarmQueue::erase(alarm->alarmQueue, alarm);
      alarm->alarmQueue = nullptr;
   }

   // Add to this core's alarm queue
   auto queue = virt_addrof(alarmData->perCoreData[OSGetCoreId()].alarmQueue);
   alarm->alarmQueue = queue;
   internal::AlarmQueue::append(queue, alarm);

   // Set the interrupt timer in processor
   // TODO: Store the last set CPU alarm time, and simply check this
   // alarm against that time to make finding the soonest alarm cheaper.
   internal::updateCpuAlarmNoALock();

   internal::releaseIdLock(alarmData->lock, alarm);
   return TRUE;
}


/**
 * Set an alarm tag which is used in OSCancelAlarms for bulk cancellation.
 */
void
OSSetAlarmTag(virt_ptr<OSAlarm> alarm,
              uint32_t group)
{
   auto alarmData = getAlarmData();
   internal::acquireIdLock(alarmData->lock, alarm);
   alarm->group = group;
   internal::releaseIdLock(alarmData->lock, alarm);
}


/**
 * Set alarm user data which is returned by OSGetAlarmUserData.
 */
void
OSSetAlarmUserData(virt_ptr<OSAlarm> alarm,
                   virt_ptr<void> data)
{
   auto alarmData = getAlarmData();
   internal::acquireIdLock(alarmData->lock, alarm);
   alarm->userData = data;
   internal::releaseIdLock(alarmData->lock, alarm);
}


/**
 * Sleep the current thread until the alarm has been triggered or cancelled.
 *
 * \return
 * Returns TRUE if alarm expired, FALSE if alarm cancelled
 */
BOOL
OSWaitAlarm(virt_ptr<OSAlarm> alarm)
{
   auto alarmData = getAlarmData();
   internal::lockScheduler();
   internal::acquireIdLock(alarmData->lock, alarm);

   decaf_check(alarm);
   decaf_check(alarm->tag == OSAlarm::Tag);

   if (alarm->state != OSAlarmState::Set) {
      internal::releaseIdLock(alarmData->lock, alarm);
      internal::unlockScheduler();
      return FALSE;
   }

   OSGetCurrentThread()->alarmCancelled = false;
   internal::sleepThreadNoLock(virt_addrof(alarm->threadQueue));

   internal::releaseIdLock(alarmData->lock, alarm);
   internal::rescheduleSelfNoLock();

   auto cancelled = OSGetCurrentThread()->alarmCancelled;
   internal::unlockScheduler();

   if (cancelled) {
      return FALSE;
   } else {
      return TRUE;
   }
}

static void
insertAlarm(virt_ptr<OSAlarm> alarm,
            AlarmCallbackFn callback)
{
   auto alarmData = getAlarmData();
   decaf_check(alarm);
   decaf_check(alarm->state == OSAlarmState::Idle || alarm->state == OSAlarmState::Invalid);
   decaf_check(alarm->link.prev == nullptr && alarm->link.next == nullptr);
   decaf_check(!OSIsInterruptEnabled());
   decaf_check(internal::isLockHeldBySomeone(alarmData->lock));

   OSGetSystemTime();
}

int
OSGetAlarmFromQueue(virt_ptr<OSAlarmQueue> queue,
                    virt_ptr<virt_ptr<OSAlarm>> outAlarm,
                    virt_ptr<virt_ptr<OSContext>> outCallbackContext,
                    uint32_t r6)
{
   auto alarmData = getAlarmData();
   OSDisableInterrupts();

   while (true) {
      internal::acquireIdLock(alarmData->lock, queue);

      if (queue->head) {
         auto alarm = queue->head;
         queue->head = alarm->link.next;

         if (!queue->head) {
            queue->tail = nullptr;
         } else {
            queue->head->link.prev = nullptr;
         }

         alarm->link.prev = nullptr;
         alarm->link.next = nullptr;
         alarm->state = OSAlarmState::Invalid;

         if (alarm->period) {
            insertAlarm(alarm, alarm->callback);

         }
      }

      internal::releaseIdLock(alarmData->lock, queue);
   }
}

static uint32_t
alarmCallbackThreadEntry(uint32_t coreId,
                         virt_ptr<void> arg2)
{
   auto alarmData = getAlarmData();
   auto queue = virt_addrof(alarmData->perCoreData[coreId].alarmQueue);
   auto cbQueue = virt_addrof(alarmData->perCoreData[coreId].callbackAlarmQueue);
   auto threadQueue = virt_addrof(alarmData->perCoreData[coreId].callbackThreadQueue);

   while (true) {
      internal::lockScheduler();
      internal::acquireIdLock(alarmData->lock, arg2);

      virt_ptr<OSAlarm> alarm = internal::AlarmQueue::popFront(cbQueue);
      if (alarm == nullptr) {
         // No alarms currently pending for callback
         internal::sleepThreadNoLock(threadQueue);
         internal::releaseIdLock(alarmData->lock, arg2);

         internal::rescheduleSelfNoLock();
         internal::unlockScheduler();
         continue;
      }

      if (alarm->period) {
         alarm->nextFire = alarm->nextFire + alarm->period;
         alarm->state = OSAlarmState::Set;
         internal::AlarmQueue::append(queue, alarm);
         alarm->alarmQueue = queue;
         internal::updateCpuAlarmNoALock();
      }

      internal::releaseIdLock(alarmData->lock, arg2);
      internal::unlockScheduler();

      if (alarm->callback) {
         cafe::invoke(cpu::this_core::state(), alarm->callback, alarm, alarm->context);
      }
   }
   return 0;
}

void
Library::initialiseAlarmStaticData()
{
   // TODO: Allocate from static memory frame allocator
   auto alarmData = virt_ptr<StaticAlarmData> { nullptr };
   Library::getStaticData()->alarmData = alarmData;

   for (auto i = 0u; i < alarmData->perCoreData.size(); ++i) {
      auto &perCoreData = alarmData->perCoreData[i];
      perCoreData.threadName = fmt::format("Alarm Thread {}", i);
      OSInitAlarmQueue(virt_addrof(perCoreData.alarmQueue));
      OSInitAlarmQueue(virt_addrof(perCoreData.callbackAlarmQueue));
      OSInitThreadQueue(virt_addrof(perCoreData.callbackThreadQueue));
   }

   alarmData->alarmCallbackThreadEntryPoint = GetInternalFunctionAddress(alarmCallbackThreadEntry);
}

void
Library::registerAlarmExports()
{
   RegisterFunctionExport(OSCancelAlarm);
   RegisterFunctionExport(OSCancelAlarms);
   RegisterFunctionExport(OSCreateAlarm);
   RegisterFunctionExport(OSCreateAlarmEx);
   RegisterFunctionExport(OSGetAlarmUserData);
   RegisterFunctionExport(OSInitAlarmQueue);
   RegisterFunctionExport(OSInitAlarmQueueEx);
   RegisterFunctionExport(OSSetAlarm);
   RegisterFunctionExport(OSSetPeriodicAlarm);
   RegisterFunctionExport(OSSetAlarmTag);
   RegisterFunctionExport(OSSetAlarmUserData);
   RegisterFunctionExport(OSWaitAlarm);

   RegisterFunctionInternal(alarmCallbackThreadEntry);
}

namespace internal
{

void
startAlarmCallbackThreads()
{
   auto alarmData = getAlarmData();

   for (auto i = 0u; i < alarmData->perCoreData.size(); ++i) {
      auto &perCoreData = alarmData->perCoreData[i];
      auto thread = virt_addrof(perCoreData.thread);
      auto stack = virt_addrof(perCoreData.threadStack);

      OSCreateThread(thread,
                     alarmData->alarmCallbackThreadEntryPoint,
                     i,
                     nullptr,
                     virt_cast<uint32_t *>(virt_addrof(perCoreData.threadStack) + perCoreData.threadStack.size()),
                     perCoreData.threadStack.size(),
                     -1,
                     static_cast<OSThreadAttributes>(1 << i));

      OSSetThreadName(thread, virt_addrof(perCoreData.threadName));
      OSResumeThread(thread);
   }
}

BOOL
setAlarmInternal(virt_ptr<OSAlarm> alarm,
                 OSTime time,
                 AlarmCallbackFn callback,
                 virt_ptr<void> userData)
{
   alarm->group = 0xFFFFFFFF;
   alarm->userData = userData;
   return OSSetAlarm(alarm, time, callback);
}

bool
cancelAlarm(virt_ptr<OSAlarm> alarm)
{
   auto alarmData = getAlarmData();
   internal::acquireIdLock(alarmData->lock, alarm);
   auto result = cancelAlarmNoAlarmLock(alarm);
   internal::releaseIdLock(alarmData->lock, alarm);
   return result == TRUE;
}

void
updateCpuAlarmNoALock()
{
   auto alarmData = getAlarmData();
   auto &queue = alarmData->perCoreData[cpu::this_core::id()].alarmQueue;
   auto next = std::chrono::steady_clock::time_point::max();

   for (virt_ptr<OSAlarm> alarm = queue.head; alarm; ) {
      auto nextAlarm = alarm->link.next;

      // Update next if its not past yet
      if (alarm->state == OSAlarmState::Set && alarm->nextFire) {
         auto nextFire = cpu::tbToTimePoint(alarm->nextFire - internal::getBaseTime());

         if (nextFire < next) {
            next = nextFire;
         }
      }

      alarm = nextAlarm;
   }

   cpu::this_core::setNextAlarm(next);
}

void
handleAlarmInterrupt(virt_ptr<OSContext> context)
{
   auto alarmData = getAlarmData();
   auto &coreAlarmData = alarmData->perCoreData[cpu::this_core::id()];
   auto queue = virt_addrof(coreAlarmData.alarmQueue);
   auto cbQueue = virt_addrof(coreAlarmData.callbackAlarmQueue);
   auto cbThreadQueue = virt_addrof(coreAlarmData.callbackThreadQueue);

   auto now = OSGetTime();
   auto next = std::chrono::time_point<std::chrono::system_clock>::max();
   bool callbacksNeeded = false;

   internal::lockScheduler();
   acquireIdLockWithCoreId(alarmData->lock);

   for (virt_ptr<OSAlarm> alarm = queue->head; alarm; ) {
      auto nextAlarm = alarm->link.next;

      // Expire it if its past its nextFire time
      if (alarm->nextFire <= now) {
         decaf_check(alarm->state == OSAlarmState::Set);

         internal::AlarmQueue::erase(queue, alarm);
         alarm->alarmQueue = nullptr;

         alarm->state = OSAlarmState::Expired;
         alarm->context = context;

         if (alarm->threadQueue.head) {
            wakeupThreadNoLock(virt_addrof(alarm->threadQueue));
            rescheduleOtherCoreNoLock();
         }

         if (alarm->group == 0xFFFFFFFF) {
            // System-internal alarm
            if (alarm->callback) {
               auto originalMask = cpu::this_core::setInterruptMask(0);
               cafe::invoke(cpu::this_core::state(), alarm->callback, alarm, context);
               cpu::this_core::setInterruptMask(originalMask);
            }
         } else {
            internal::AlarmQueue::append(cbQueue, alarm);
            alarm->alarmQueue = cbQueue;

            wakeupThreadNoLock(cbThreadQueue);
         }
      }

      alarm = nextAlarm;
   }

   internal::updateCpuAlarmNoALock();

   releaseIdLockWithCoreId(alarmData->lock);
   internal::unlockScheduler();
}

} // namespace internal

} // namespace coreinit

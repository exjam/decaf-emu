#include "ios_scheduler.h"
#include "ios_syscalls.h"

#include <array>
#include <mutex>

namespace ios
{

static thread_local
Thread *sCurrentThreadContext = nullptr;

static std::mutex
sSchedulerMutex;

static std::array<Thread, 180>
sThreads;

namespace syscalls
{

Result<ThreadID>
createThread(ThreadEntryFn entry,
             void *context,
             void *stackTop,
             size_t stackSize,
             int priority,
             uint32_t flags)
{
   std::unique_lock<std::mutex> lock { sSchedulerMutex };
   Thread *thread = nullptr;

   if (sCurrentThreadContext && priority > sCurrentThreadContext->minPriority) {
      // Cannot create thread with priority higher than current priority
      return IOSError::Invalid;
   }

   // Find a free thread
   for (auto i = 0u; i < sThreads.size(); ++i) {
      if (sThreads[i].state == ThreadState::Available) {
         thread = &sThreads[i];
         thread->id = i;
         break;
      }
   }

   if (!thread) {
      // Maximum number of threads running
      return IOSError::Invalid;
   }

   if (sCurrentThreadContext) {
      thread->pid = sCurrentThreadContext->pid;
   }

   thread->state = ThreadState::Stopped;
   thread->minPriority = priority;
   thread->maxPriority = priority;
   return thread->id;
}

Result<IOSError>
joinThread(ThreadID id,
           uint32_t *returnedValue)
{
   if (!id) {
      id = sCurrentThreadContext->id;
   }
}

Result<IOSError>
startThread(ThreadID id)
{
   return IOSError::Invalid;
}

Result<ThreadID>
getThreadId()
{
   if (!sCurrentThreadContext) {
      return IOSError::Invalid;
   }

   return sCurrentThreadContext->id;
}

Result<IOSProcessID>
getProcessId()
{
   if (!sCurrentThreadContext) {
      return IOSError::Invalid;
   }

   return sCurrentThreadContext->pid;
}

} // namespace syscalls

} // namespace ios

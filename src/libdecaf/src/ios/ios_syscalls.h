#pragma once
#include "ios_error.h"
#include "ios_thread.h"
#include "ios_process.h"
#include "ios_result.h"

namespace ios
{

namespace syscalls
{

Result<ThreadID>
createThread(ThreadEntryFn entry,
             void *context,
             void *stackTop,
             size_t stackSize,
             int priority,
             uint32_t flags);

Result<IOSError>
joinThread(ThreadID id,
           uint32_t *returnedValue);

Result<IOSError>
cancelThread(ThreadID id,
             uint32_t returnValue);

Result<ThreadID>
getThreadId();

Result<IOSError>
startThread(ThreadID id);

Result<IOSError>
suspendThread(ThreadID id);

Result<IOSError>
yieldThread();

Result<ThreadPriority>
getThreadPriority(ThreadID id);

Result<IOSError>
setThreadPriority(ThreadID id,
                  ThreadPriority priority);

} // namespace syscalls

} // namespace ios

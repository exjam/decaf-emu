#include "ios_fs.h"
#include "ios_fs_log.h"
#include "ios_fs_fsa_async_task.h"
#include "ios_fs_fsa_thread.h"
#include "ios_fs_service_thread.h"

#include "decaf_log.h"
#include "ios/kernel/ios_kernel_heap.h"
#include "ios/kernel/ios_kernel_process.h"
#include "ios/kernel/ios_kernel_thread.h"

#include <common/log.h>

namespace ios::fs
{

constexpr auto LocalHeapSize = 0x56000u;
constexpr auto CrossHeapSize = 0x1D80000u;

static phys_ptr<void>
sLocalHeapBuffer = nullptr;

namespace internal
{

Logger fsLog = { };

static void
initialiseStaticData()
{
   sLocalHeapBuffer = kernel::allocProcessLocalHeap(LocalHeapSize);
}

} // namespace internal

Error
processEntryPoint(phys_ptr<void> context)
{
   // Initialise logger
   internal::fsLog = decaf::makeLogger("IOS_FS");

   // Initialise static memory
   internal::initialiseStaticData();
   internal::initialiseStaticFsaAsyncTaskData();
   internal::initialiseStaticFsaThreadData();
   internal::initialiseStaticServiceThreadData();

   // Initialise process heaps
   auto error = kernel::IOS_CreateLocalProcessHeap(sLocalHeapBuffer, LocalHeapSize);
   if (error < Error::OK) {
      gLog->error("Failed to create local process heap, error = {}.", error);
      return error;
   }

   error = kernel::IOS_CreateCrossProcessHeap(CrossHeapSize);
   if (error < Error::OK) {
      gLog->error("Failed to create cross process heap, error = {}.", error);
      return error;
   }

   // TODO: Get clock info from bsp

   // TODO: Start dk thread

   // TODO: Start odm thread

   // Start FSA async task thread.
   error = internal::startFsaAsyncTaskThread();
   if (error < Error::OK) {
      gLog->error("Failed to start FSA async task thread");
      return error;
   }

   // Start FSA thread.
   error = internal::startFsaThread();
   if (error < Error::OK) {
      gLog->error("Failed to start FSA thread");
      return error;
   }

   // Start service thread.
   error = internal::startServiceThread();
   if (error < Error::OK) {
      gLog->error("Failed to start Service thread");
      return error;
   }

   // TODO: Start ramdisk service thread.

   // Suspend current thread.
   error = kernel::IOS_GetCurrentThreadId();
   if (error < Error::OK) {
      gLog->error("Failed to get current thread id");
      return error;
   }

   auto threadId = static_cast<kernel::ThreadId>(error);
   error = kernel::IOS_SuspendThread(threadId);
   if (error < Error::OK) {
      gLog->error("Failed to suspend root fs thread");
      return error;
   }

   return Error::OK;
}

} // namespace ios

#include "coreinit.h"
#include "coreinit_core.h"
#include "coreinit_exception.h"
#include "coreinit_interrupts.h"
#include "coreinit_thread.h"
#include "cafe/kernel/cafe_kernel_exception.h"

namespace cafe::coreinit
{

struct StaticExceptionData
{
   be2_array<OSExceptionCallbackFn, OSGetCoreCount()> dsiCallback;
   be2_array<OSExceptionCallbackFn, OSGetCoreCount()> isiCallback;
   be2_array<OSExceptionCallbackFn, OSGetCoreCount()> programCallback;
   be2_array<OSExceptionCallbackFn, OSGetCoreCount()> alignCallback;
};

virt_ptr<StaticExceptionData>
getExceptionData()
{
}

static void
unkSyscall0x7A00(kernel::ExceptionType type,
                 uint32_t value)
{
   // TODO: I think this might be clear & enable or something?
}

OSExceptionCallbackFn
OSSetExceptionCallback(OSExceptionType type,
                       OSExceptionCallbackFn callback)
{
   return OSSetExceptionCallbackEx(OSExceptionMode::Thread, type, callback);
}

OSExceptionCallbackFn
OSSetExceptionCallbackEx(OSExceptionMode mode,
                         OSExceptionType type,
                         OSExceptionCallbackFn callback)
{
   // Only certain exceptions are supported for callback
   if (type != OSExceptionType::DSI &&
       type != OSExceptionType::ISI &&
       type != OSExceptionType::Alignment &&
       type != OSExceptionType::Program &&
       type != OSExceptionType::PerformanceMonitor) {
      return nullptr;
   }

   auto restoreInterruptValue = OSDisableInterrupts();
   auto previousCallback = internal::getExceptionCallback(mode, type);
   internal::setExceptionCallback(mode, type, callback);

   if (mode == OSExceptionMode::Thread &&
       type == OSExceptionType::PerformanceMonitor) {

   }

   OSRestoreInterrupts(restoreInterruptValue);
   return previousCallback;
}

namespace internal
{

void
initialiseExceptionHandlers()
{

}

OSExceptionCallbackFn
getExceptionCallback(OSExceptionMode mode,
                     OSExceptionType type)
{
   auto globalData = getExceptionData();
   auto thread = OSGetCurrentThread();
   auto core = OSGetCoreId();
   auto callback = OSExceptionCallbackFn { nullptr };

   if (mode == OSExceptionMode::Thread ||
       mode == OSExceptionMode::ThreadAllCores ||
       mode == OSExceptionMode::System) {
      switch (type) {
      case OSExceptionType::DSI:
         callback = thread->dsiCallback[core];
         break;
      case OSExceptionType::ISI:
         callback = thread->isiCallback[core];
         break;
      case OSExceptionType::Alignment:
         callback = thread->alignCallback[core];
         break;
      case OSExceptionType::Program:
         callback = thread->programCallback[core];
         break;
      case OSExceptionType::PerformanceMonitor:
         callback = thread->perfCallback[core];
         break;
      }

      if (callback ||
          mode != OSExceptionMode::System) {
         return callback;
      }
   }

   if (mode == OSExceptionMode::Global ||
       mode == OSExceptionMode::GlobalAllCores ||
       mode == OSExceptionMode::System) {
      switch (type) {
      case OSExceptionType::DSI:
         callback = globalData->dsiCallback[core];
      case OSExceptionType::ISI:
         callback = globalData->isiCallback[core];
      case OSExceptionType::Alignment:
         callback = globalData->alignCallback[core];
      case OSExceptionType::Program:
         callback = globalData->programCallback[core];
      default:
         return nullptr;
      }
   }

   return callback;
}

void
setExceptionCallback(OSExceptionMode mode,
                     OSExceptionType type,
                     OSExceptionCallbackFn callback)
{
   auto globalData = getExceptionData();
   auto thread = OSGetCurrentThread();
   auto core = OSGetCoreId();

   if (mode == OSExceptionMode::Thread) {
      switch (type) {
      case OSExceptionType::DSI:
         thread->dsiCallback[core] = callback;
         unkSyscall0x7A00(kernel::ExceptionType::DSI, 0);
         break;
      case OSExceptionType::ISI:
         thread->isiCallback[core] = callback;
         unkSyscall0x7A00(kernel::ExceptionType::ISI, 0);
         break;
      case OSExceptionType::Alignment:
         thread->alignCallback[core] = callback;
         unkSyscall0x7A00(kernel::ExceptionType::Alignment, 0);
         break;
      case OSExceptionType::Program:
         thread->programCallback[core] = callback;
         unkSyscall0x7A00(kernel::ExceptionType::Program, 0);
         break;
      case OSExceptionType::PerformanceMonitor:
         thread->perfCallback[core] = callback;
         break;
      }
   } else if (mode == OSExceptionMode::ThreadAllCores) {
      switch (type) {
      case OSExceptionType::DSI:
         thread->dsiCallback[0] = callback;
         thread->dsiCallback[1] = callback;
         thread->dsiCallback[2] = callback;
         unkSyscall0x7A00(kernel::ExceptionType::DSI, 0);
         break;
      case OSExceptionType::ISI:
         thread->isiCallback[0] = callback;
         thread->isiCallback[1] = callback;
         thread->isiCallback[2] = callback;
         unkSyscall0x7A00(kernel::ExceptionType::ISI, 0);
         break;
      case OSExceptionType::Alignment:
         thread->alignCallback[0] = callback;
         thread->alignCallback[1] = callback;
         thread->alignCallback[2] = callback;
         unkSyscall0x7A00(kernel::ExceptionType::Alignment, 0);
         break;
      case OSExceptionType::Program:
         thread->programCallback[0] = callback;
         thread->programCallback[1] = callback;
         thread->programCallback[2] = callback;
         unkSyscall0x7A00(kernel::ExceptionType::Program, 0);
         break;
      case OSExceptionType::PerformanceMonitor:
         thread->perfCallback[0] = callback;
         thread->perfCallback[1] = callback;
         thread->perfCallback[2] = callback;
         break;
      }
   } else if (mode == OSExceptionMode::Global) {
      switch (type) {
      case OSExceptionType::DSI:
         globalData->dsiCallback[core] = callback;
         unkSyscall0x7A00(kernel::ExceptionType::DSI, 0);
         break;
      case OSExceptionType::ISI:
         globalData->isiCallback[core] = callback;
         unkSyscall0x7A00(kernel::ExceptionType::ISI, 0);
         break;
      case OSExceptionType::Alignment:
         globalData->alignCallback[core] = callback;
         unkSyscall0x7A00(kernel::ExceptionType::Alignment, 0);
         break;
      case OSExceptionType::Program:
         globalData->programCallback[core] = callback;
         unkSyscall0x7A00(kernel::ExceptionType::Program, 0);
         break;
      }
   } else if (mode == OSExceptionMode::GlobalAllCores) {
      switch (type) {
      case OSExceptionType::DSI:
         globalData->dsiCallback[0] = callback;
         globalData->dsiCallback[1] = callback;
         globalData->dsiCallback[2] = callback;
         unkSyscall0x7A00(kernel::ExceptionType::DSI, 0);
         break;
      case OSExceptionType::ISI:
         globalData->isiCallback[0] = callback;
         globalData->isiCallback[1] = callback;
         globalData->isiCallback[2] = callback;
         unkSyscall0x7A00(kernel::ExceptionType::ISI, 0);
         break;
      case OSExceptionType::Alignment:
         globalData->alignCallback[0] = callback;
         globalData->alignCallback[1] = callback;
         globalData->alignCallback[2] = callback;
         unkSyscall0x7A00(kernel::ExceptionType::Alignment, 0);
         break;
      case OSExceptionType::Program:
         globalData->programCallback[0] = callback;
         globalData->programCallback[1] = callback;
         globalData->programCallback[2] = callback;
         unkSyscall0x7A00(kernel::ExceptionType::Program, 0);
         break;
      }
   }
}

} // namespace internal

} // namespace cafe::coreinit

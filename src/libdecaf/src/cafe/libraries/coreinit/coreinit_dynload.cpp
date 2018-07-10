#include "coreinit.h"
#include "coreinit_dynload.h"
#include "coreinit_mutex.h"
#include "coreinit_systemheap.h"

namespace cafe::coreinit
{

struct StaticDynLoadData
{
   be2_struct<OSMutex> mutex;

   be2_val<OSDynLoad_AllocFn> allocFn;
   be2_val<OSDynLoad_FreeFn> freeFn;

   be2_val<BOOL> tlsAllocLocked;
   be2_val<OSDynLoad_AllocFn> tlsAllocFn;
   be2_val<OSDynLoad_FreeFn> tlsFreeFn;

   be2_virt_ptr<OSDynLoad_NotifyCallback> notifyCallbacks;
};

static virt_ptr<StaticDynLoadData>
sDynLoadData = nullptr;


/**
 * Registers a callback to be called when an RPL is loaded or unloaded.
 */
OSDynLoad_Error
OSDynLoad_AddNotifyCallback(OSDynLoad_NotifyCallbackFn notifyFn,
                            virt_ptr<void> userArg1)
{
   if (!notifyFn) {
      return OSDynLoad_Error::InvalidAllocatorPtr;
   }

   auto notifyCallback = virt_cast<OSDynLoad_NotifyCallback *>(
      OSAllocFromSystem(sizeof(OSDynLoad_NotifyCallback), 4));
   if (!notifyCallback) {
      return OSDynLoad_Error::OutOfSysMemory;
   }

   OSLockMutex(virt_addrof(sDynLoadData->mutex));
   notifyCallback->notifyFn = notifyFn;
   notifyCallback->userArg1 = userArg1;
   notifyCallback->next = sDynLoadData->notifyCallbacks;

   sDynLoadData->notifyCallbacks = notifyCallback;
   OSUnlockMutex(virt_addrof(sDynLoadData->mutex));
   return OSDynLoad_Error::OK;
}


/**
 * Deletes a callback previously registered by OSDynLoad_AddNotifyCallback.
 */
void
OSDynLoad_DelNotifyCallback(OSDynLoad_NotifyCallbackFn notifyFn,
                            virt_ptr<void> userArg1)
{
   if (!notifyFn) {
      return;
   }

   OSLockMutex(virt_addrof(sDynLoadData->mutex));

   // Find the callback
   auto prevCallback = virt_ptr<OSDynLoad_NotifyCallback> { nullptr };
   auto callback = sDynLoadData->notifyCallbacks;
   while (callback) {
      if (callback->notifyFn == notifyFn && callback->userArg1 == userArg1) {
         break;
      }

      prevCallback = callback;
      callback = callback->next;
   }

   if (callback) {
      // Erase it from linked list
      if (prevCallback) {
         prevCallback->next = callback->next;
      } else {
         sDynLoadData->notifyCallbacks = callback->next;
      }
   }

   OSUnlockMutex(virt_addrof(sDynLoadData->mutex));

   // Free the callback
   if (callback) {
      OSFreeToSystem(callback);
   }
}


/**
 * Retrieve the allocator functions set by OSDynLoad_SetAllocator.
 */
OSDynLoad_Error
OSDynLoad_GetAllocator(virt_ptr<OSDynLoad_AllocFn> outAllocFn,
                       virt_ptr<OSDynLoad_FreeFn> outFreeFn)
{
   OSLockMutex(virt_addrof(sDynLoadData->mutex));

   if (outAllocFn) {
      *outAllocFn = sDynLoadData->allocFn;
   }

   if (outFreeFn) {
      *outFreeFn = sDynLoadData->freeFn;
   }

   OSUnlockMutex(virt_addrof(sDynLoadData->mutex));
   return OSDynLoad_Error::OK;
}


/**
 * Set the allocator which controls allocation of RPL segments.
 */
OSDynLoad_Error
OSDynLoad_SetAllocator(OSDynLoad_AllocFn allocFn,
                       OSDynLoad_FreeFn freeFn)
{
   if (!allocFn || !freeFn) {
      return OSDynLoad_Error::InvalidAllocatorPtr;
   }

   OSLockMutex(virt_addrof(sDynLoadData->mutex));
   sDynLoadData->allocFn = allocFn;
   sDynLoadData->freeFn = freeFn;
   OSUnlockMutex(virt_addrof(sDynLoadData->mutex));
   return OSDynLoad_Error::OK;
}


/**
 * Retrieve the allocator functions set by OSDynLoad_SetTLSAllocator.
 */
OSDynLoad_Error
OSDynLoad_GetTLSAllocator(virt_ptr<OSDynLoad_AllocFn> outAllocFn,
                          virt_ptr<OSDynLoad_FreeFn> outFreeFn)
{
   OSLockMutex(virt_addrof(sDynLoadData->mutex));

   if (outAllocFn) {
      *outAllocFn = sDynLoadData->tlsAllocFn;
   }

   if (outFreeFn) {
      *outFreeFn = sDynLoadData->tlsFreeFn;
   }

   OSUnlockMutex(virt_addrof(sDynLoadData->mutex));
   return OSDynLoad_Error::OK;
}


/**
 * Set the allocator which controls allocation of TLS memory.
 */
OSDynLoad_Error
OSDynLoad_SetTLSAllocator(OSDynLoad_AllocFn allocFn,
                          OSDynLoad_FreeFn freeFn)
{
   if (!allocFn || !freeFn) {
      return OSDynLoad_Error::InvalidAllocatorPtr;
   }

   if (sDynLoadData->tlsAllocLocked) {
      return OSDynLoad_Error::TLSAllocatorLocked;
   }

   OSLockMutex(virt_addrof(sDynLoadData->mutex));
   sDynLoadData->tlsAllocFn = allocFn;
   sDynLoadData->tlsFreeFn = freeFn;
   OSUnlockMutex(virt_addrof(sDynLoadData->mutex));
   return OSDynLoad_Error::OK;
}

OSDynLoad_Error
OSDynLoad_Acquire(virt_ptr<const char> modulePath,
                  virt_ptr<OSDynLoad_ModuleHandle> outModuleHandle)
{
   return OSDynLoad_Error::OutOfMemory;
}

OSDynLoad_Error
OSDynLoad_AcquireContainingModule(virt_ptr<void> ptr,
                                  OSDynLoad_SectionType sectionType,
                                  virt_ptr<OSDynLoad_ModuleHandle> outHandle)
{
   return OSDynLoad_Error::OK;
}


/**
 * Find an export from a library handle.
 */
OSDynLoad_Error
OSDynLoad_FindExport(OSDynLoad_ModuleHandle moduleHandle,
                     BOOL isData,
                     virt_ptr<const char> name,
                     virt_ptr<virt_addr> outAddr)
{
   return OSDynLoad_Error::OutOfMemory;
}

OSDynLoad_Error
OSDynLoad_GetModuleName(OSDynLoad_ModuleHandle moduleHandle,
                        virt_ptr<char> buffer,
                        uint32_t bufferSize)
{
   return OSDynLoad_Error::OutOfMemory;
}


/**
* Check if a module is loaded and return the handle of a dynamic library.
*/
OSDynLoad_Error
OSDynLoad_IsModuleLoaded(virt_ptr<const char> name,
                         virt_ptr<OSDynLoad_ModuleHandle> outHandle)
{
   return OSDynLoad_Error::OutOfMemory;
}


OSDynLoad_Error
OSDynLoad_Release(OSDynLoad_ModuleHandle moduleHandle)
{
   return OSDynLoad_Error::OutOfMemory;
}

uint32_t
OSGetSymbolName(uint32_t address,
                virt_ptr<char> buffer,
                uint32_t bufferSize)
{
   if (buffer && bufferSize > 0) {
      *buffer = char { 0 };
   }

   return 0;
}

/**
 * __tls_get_addr
 * Gets the TLS data for tls_index.
 */
virt_ptr<void>
tls_get_addr(virt_ptr<tls_index> index)
{
   /*
   auto thread = OSGetCurrentThread();
   auto module = kernel::getTLSModule(index->moduleIndex);
   decaf_check(module);

   if (thread->tlsSectionCount <= index->moduleIndex) {
      auto oldSections = thread->tlsSections;
      auto oldCount = thread->tlsSectionCount;
      thread->tlsSectionCount = index->moduleIndex + 1;

      // Allocate and zero new tlsSections
      internal::dynLoadTLSAlloc(thread->tlsSectionCount * sizeof(OSTLSSection), 4, virt_addrof(thread->tlsSections));
      memset(virt_addrof(thread->tlsSections[oldCount]),
             0,
             sizeof(OSTLSSection) * (thread->tlsSectionCount - oldCount));

      // Copy and free old tlsSections
      if (oldSections) {
         memcpy(thread->tlsSections, oldSections, oldCount * sizeof(OSTLSSection));
         internal::dynLoadTLSFree(oldSections);
      }
   }

   auto &section = thread->tlsSections[index->moduleIndex];

   if (!section.data) {
      // Allocate and copy TLS data
      internal::dynLoadTLSAlloc(module->tlsSize, 1u << module->tlsAlignShift, &section.data);
      memcpy(section.data, module->tlsBase, module->tlsSize);
   }

   return mem::translate<void *>(section.data.getAddress() + index->offset);
   */
   return nullptr;
}

namespace internal
{

void
dynLoadTlsFree(virt_ptr<OSThread> thread)
{
   if (thread->tlsSectionCount) {
      for (auto i = 0u; i < thread->tlsSectionCount; ++i) {
         cafe::invoke(cpu::this_core::state(),
                      sDynLoadData->tlsFreeFn,
                      thread->tlsSections[i].data);
      }

      cafe::invoke(cpu::this_core::state(),
                   sDynLoadData->tlsFreeFn,
                   virt_cast<void *>(thread->tlsSections));
   }
}

void
initialiseDynLoad()
{

}

} // namespace internal

void
Library::registerDynLoadSymbols()
{
   RegisterFunctionExport(OSDynLoad_AddNotifyCallback);
   RegisterFunctionExport(OSDynLoad_DelNotifyCallback);
   RegisterFunctionExportName("OSDynLoad_AddNofifyCallback", // covfefe
                              OSDynLoad_AddNotifyCallback);

   RegisterFunctionExport(OSDynLoad_GetAllocator);
   RegisterFunctionExport(OSDynLoad_SetAllocator);
   RegisterFunctionExport(OSDynLoad_GetTLSAllocator);
   RegisterFunctionExport(OSDynLoad_SetTLSAllocator);

   RegisterFunctionExport(OSDynLoad_Acquire);
   RegisterFunctionExport(OSDynLoad_AcquireContainingModule);
   RegisterFunctionExport(OSDynLoad_FindExport);
   RegisterFunctionExport(OSDynLoad_GetModuleName);
   RegisterFunctionExport(OSDynLoad_IsModuleLoaded);
   RegisterFunctionExport(OSDynLoad_Release);

   RegisterFunctionExport(OSGetSymbolName);
   RegisterFunctionExportName("__tls_get_addr", tls_get_addr);

   RegisterDataInternal(sDynLoadData);
}

} // namespace cafe::coreinit

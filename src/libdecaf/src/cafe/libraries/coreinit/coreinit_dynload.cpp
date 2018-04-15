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
getDynLoadData()
{
   return Library::getStaticData()->dynLoadData;
}


/**
 * Registers a callback to be called when an RPL is loaded or unloaded.
 */
OSDynLoad_Error
OSDynLoad_AddNotifyCallback(OSDynLoad_NotifyCallbackFn notifyFn,
                            virt_ptr<void> userArg1)
{
   auto dynLoadData = getDynLoadData();
   if (!notifyFn) {
      return OSDynLoad_Error::InvalidAllocatorPtr;
   }

   auto notifyCallback = virt_cast<OSDynLoad_NotifyCallback *>(
      OSAllocFromSystem(sizeof(OSDynLoad_NotifyCallback), 4));
   if (!notifyCallback) {
      return OSDynLoad_Error::OutOfSysMemory;
   }

   OSLockMutex(virt_addrof(dynLoadData->mutex));
   notifyCallback->notifyFn = notifyFn;
   notifyCallback->userArg1 = userArg1;
   notifyCallback->next = dynLoadData->notifyCallbacks;

   dynLoadData->notifyCallbacks = notifyCallback;
   OSUnlockMutex(virt_addrof(dynLoadData->mutex));
   return OSDynLoad_Error::OK;
}


/**
 * Deletes a callback previously registered by OSDynLoad_AddNotifyCallback.
 */
void
OSDynLoad_DelNotifyCallback(OSDynLoad_NotifyCallbackFn notifyFn,
                            virt_ptr<void> userArg1)
{
   auto dynLoadData = getDynLoadData();
   if (!notifyFn) {
      return;
   }

   OSLockMutex(virt_addrof(dynLoadData->mutex));

   // Find the callback
   auto prevCallback = virt_ptr<OSDynLoad_NotifyCallback> { nullptr };
   auto callback = dynLoadData->notifyCallbacks;
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
         dynLoadData->notifyCallbacks = callback->next;
      }
   }

   OSUnlockMutex(virt_addrof(dynLoadData->mutex));

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
   auto dynLoadData = getDynLoadData();
   OSLockMutex(virt_addrof(dynLoadData->mutex));

   if (outAllocFn) {
      *outAllocFn = dynLoadData->allocFn;
   }

   if (outFreeFn) {
      *outFreeFn = dynLoadData->freeFn;
   }

   OSUnlockMutex(virt_addrof(dynLoadData->mutex));
   return OSDynLoad_Error::OK;
}


/**
 * Set the allocator which controls allocation of RPL segments.
 */
OSDynLoad_Error
OSDynLoad_SetAllocator(OSDynLoad_AllocFn allocFn,
                       OSDynLoad_FreeFn freeFn)
{
   auto dynLoadData = getDynLoadData();
   if (!allocFn || !freeFn) {
      return OSDynLoad_Error::InvalidAllocatorPtr;
   }

   OSLockMutex(virt_addrof(dynLoadData->mutex));
   dynLoadData->allocFn = allocFn;
   dynLoadData->freeFn = freeFn;
   OSUnlockMutex(virt_addrof(dynLoadData->mutex));
   return OSDynLoad_Error::OK;
}


/**
 * Retrieve the allocator functions set by OSDynLoad_SetTLSAllocator.
 */
OSDynLoad_Error
OSDynLoad_GetTLSAllocator(virt_ptr<OSDynLoad_AllocFn> outAllocFn,
                          virt_ptr<OSDynLoad_FreeFn> outFreeFn)
{
   auto dynLoadData = getDynLoadData();
   OSLockMutex(virt_addrof(dynLoadData->mutex));

   if (outAllocFn) {
      *outAllocFn = dynLoadData->tlsAllocFn;
   }

   if (outFreeFn) {
      *outFreeFn = dynLoadData->tlsFreeFn;
   }

   OSUnlockMutex(virt_addrof(dynLoadData->mutex));
   return OSDynLoad_Error::OK;
}


/**
 * Set the allocator which controls allocation of TLS memory.
 */
OSDynLoad_Error
OSDynLoad_SetTLSAllocator(OSDynLoad_AllocFn allocFn,
                          OSDynLoad_FreeFn freeFn)
{
   auto dynLoadData = getDynLoadData();
   if (!allocFn || !freeFn) {
      return OSDynLoad_Error::InvalidAllocatorPtr;
   }

   if (dynLoadData->tlsAllocLocked) {
      return OSDynLoad_Error::TLSAllocatorLocked;
   }

   OSLockMutex(virt_addrof(dynLoadData->mutex));
   dynLoadData->tlsAllocFn = allocFn;
   dynLoadData->tlsFreeFn = freeFn;
   OSUnlockMutex(virt_addrof(dynLoadData->mutex));
   return OSDynLoad_Error::OK;
}


void
Library::initialiseDynLoadStaticData()
{
   auto dynLoadData = allocStaticData<StaticDynLoadData>();
   getStaticData()->dynLoadData = dynLoadData;
}

void
Library::registerDynLoadExports()
{
   RegisterFunctionExport(OSDynLoad_AddNotifyCallback);
   RegisterFunctionExport(OSDynLoad_DelNotifyCallback);
   RegisterFunctionExportName("OSDynLoad_AddNofifyCallback", // covfefe
                              OSDynLoad_AddNotifyCallback);

   RegisterFunctionExport(OSDynLoad_GetAllocator);
   RegisterFunctionExport(OSDynLoad_SetAllocator);
   RegisterFunctionExport(OSDynLoad_GetTLSAllocator);
   RegisterFunctionExport(OSDynLoad_SetTLSAllocator);
}

} // namespace cafe::coreinit

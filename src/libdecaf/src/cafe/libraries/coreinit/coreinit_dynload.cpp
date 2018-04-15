#include "coreinit.h"
#include "coreinit_dynload.h"
#include "coreinit_mutex.h"

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
};

static virt_ptr<StaticDynLoadData>
getDynLoadData()
{
   return Library::getStaticData()->dynLoadData;
}


/**
 * Retrieve the allocator functions set by OSDynLoad_SetAllocator.
 */
OSDynLoadError
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
   return OSDynLoadError::OK;
}


/**
 * Set the allocator which controls allocation of RPL segments.
 */
OSDynLoadError
OSDynLoad_SetAllocator(OSDynLoad_AllocFn allocFn,
                       OSDynLoad_FreeFn freeFn)
{
   auto dynLoadData = getDynLoadData();
   if (!allocFn || !freeFn) {
      return OSDynLoadError::InvalidAllocatorPtr;
   }

   OSLockMutex(virt_addrof(dynLoadData->mutex));
   dynLoadData->allocFn = allocFn;
   dynLoadData->freeFn = freeFn;
   OSUnlockMutex(virt_addrof(dynLoadData->mutex));
   return OSDynLoadError::OK;
}


/**
 * Retrieve the allocator functions set by OSDynLoad_SetTLSAllocator.
 */
OSDynLoadError
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
   return OSDynLoadError::OK;
}


/**
 * Set the allocator which controls allocation of TLS memory.
 */
OSDynLoadError
OSDynLoad_SetTLSAllocator(OSDynLoad_AllocFn allocFn,
                          OSDynLoad_FreeFn freeFn)
{
   auto dynLoadData = getDynLoadData();
   if (!allocFn || !freeFn) {
      return OSDynLoadError::InvalidAllocatorPtr;
   }

   if (dynLoadData->tlsAllocLocked) {
      return OSDynLoadError::TLSAllocatorLocked;
   }

   OSLockMutex(virt_addrof(dynLoadData->mutex));
   dynLoadData->tlsAllocFn = allocFn;
   dynLoadData->tlsFreeFn = freeFn;
   OSUnlockMutex(virt_addrof(dynLoadData->mutex));
   return OSDynLoadError::OK;
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
   RegisterFunctionExport(OSDynLoad_GetAllocator);
   RegisterFunctionExport(OSDynLoad_SetAllocator);
   RegisterFunctionExport(OSDynLoad_GetTLSAllocator);
   RegisterFunctionExport(OSDynLoad_SetTLSAllocator);
}

} // namespace cafe::coreinit

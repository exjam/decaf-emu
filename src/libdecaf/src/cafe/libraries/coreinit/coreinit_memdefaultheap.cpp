#include "coreinit.h"
#include "coreinit_dynload.h"
#include "coreinit_memdefaultheap.h"
#include "coreinit_memexpheap.h"
#include "coreinit_memframeheap.h"
#include "coreinit_memheap.h"
#include "coreinit_memory.h"

#include "cafe/cafe_stackobject.h"
#include "cafe/cafe_ppc_interface_invoke.h"

#include <libcpu/cpu.h>

namespace cafe::coreinit
{

using MEMAllocFromDefaultHeapFn = virt_func_ptr<virt_ptr<void>(uint32_t size)>;
using MEMAllocFromDefaultHeapExFn = virt_func_ptr<virt_ptr<void>(uint32_t size, int32_t align)>;
using MEMFreeToDefaultHeapFn = virt_func_ptr<void(virt_ptr<void>)>;

struct StaticDefaultHeapData
{
   be2_val<MEMHeapHandle> defaultHeapHandle;
   be2_val<uint32_t> defaultHeapAllocCount;
   be2_val<uint32_t> defaultHeapFreeCount;

   be2_val<MEMAllocFromDefaultHeapFn> defaultAllocFromDefaultHeap;
   be2_val<MEMAllocFromDefaultHeapExFn> defaultAllocFromDefaultHeapEx;
   be2_val<MEMFreeToDefaultHeapFn> defaultFreeToDefaultHeap;

   be2_val<OSDynLoad_AllocFn> defaultDynLoadAlloc;
   be2_val<OSDynLoad_FreeFn> defaultDynLoadFree;

   be2_val<MEMAllocFromDefaultHeapFn> allocFromDefaultHeap;
   be2_val<MEMAllocFromDefaultHeapExFn> allocFromDefaultHeapEx;
   be2_val<MEMFreeToDefaultHeapFn> freeToDefaultHeap;
};

static virt_ptr<StaticDefaultHeapData>
getDefaultHeapData()
{
   return Library::getStaticData()->defaultHeapData;
}

static virt_ptr<void>
defaultAllocFromDefaultHeap(uint32_t size)
{
   auto defaultHeapData = getDefaultHeapData();
   return MEMAllocFromExpHeapEx(defaultHeapData->defaultHeapHandle, size, 0x40u);
}

static virt_ptr<void>
defaultAllocFromDefaultHeapEx(uint32_t size,
                              int32_t alignment)
{
   auto defaultHeapData = getDefaultHeapData();
   return MEMAllocFromExpHeapEx(defaultHeapData->defaultHeapHandle, size, alignment);
}

static void
defaultFreeToDefaultHeap(virt_ptr<void> block)
{
   auto defaultHeapData = getDefaultHeapData();
   return MEMFreeToExpHeap(defaultHeapData->defaultHeapHandle, block);
}

static OSDynLoadError
defaultDynLoadAlloc(int32_t size,
                    int32_t align,
                    virt_ptr<virt_ptr<void>> outPtr)
{
   if (!outPtr) {
      return OSDynLoadError::InvalidAllocatorPtr;
   }

   if (align >= 0 && align < 4) {
      align = 4;
   } else if (align < 0 && align > -4) {
      align = -4;
   }

   auto ptr = MEMAllocFromDefaultHeapEx(size, align);
   *outPtr = ptr;

   if (!ptr) {
      return OSDynLoadError::OutOfMemory;
   }

   return OSDynLoadError::OK;
}

static void
defaultDynLoadFree(virt_ptr<void> ptr)
{
   MEMFreeToDefaultHeap(ptr);
}

void
CoreInitDefaultHeap(virt_ptr<MEMHeapHandle> outHeapHandleMEM1,
                    virt_ptr<MEMHeapHandle> outHeapHandleFG,
                    virt_ptr<MEMHeapHandle> outHeapHandleMEM2)
{
   StackObject<virt_addr> addr;
   StackObject<uint32_t> size;

   auto defaultHeapData = getDefaultHeapData();
   defaultHeapData->allocFromDefaultHeap = defaultHeapData->defaultAllocFromDefaultHeap;
   defaultHeapData->allocFromDefaultHeapEx = defaultHeapData->defaultAllocFromDefaultHeapEx;
   defaultHeapData->freeToDefaultHeap = defaultHeapData->defaultFreeToDefaultHeap;

   defaultHeapData->defaultHeapAllocCount = 0u;
   defaultHeapData->defaultHeapFreeCount = 0u;

   *outHeapHandleMEM1 = nullptr;
   *outHeapHandleFG = nullptr;
   *outHeapHandleMEM2 = nullptr;

   if (OSGetForegroundBucket(nullptr, nullptr)) {
      OSGetMemBound(OSMemoryType::MEM1, addr, size);
      *outHeapHandleMEM1 = MEMCreateFrmHeapEx(virt_cast<void *>(*addr), *size, 0);

      OSGetForegroundBucketFreeArea(addr, size);
      *outHeapHandleFG = MEMCreateFrmHeapEx(virt_cast<void *>(*addr), *size, 0);
   }

   OSGetMemBound(OSMemoryType::MEM2, addr, size);
   defaultHeapData->defaultHeapHandle =
      MEMCreateExpHeapEx(virt_cast<void *>(*addr),
                         *size,
                         MEMHeapFlags::ThreadSafe);
   *outHeapHandleMEM2 = defaultHeapData->defaultHeapHandle;

   OSDynLoad_SetAllocator(defaultHeapData->defaultDynLoadAlloc,
                          defaultHeapData->defaultDynLoadFree);

   OSDynLoad_SetTLSAllocator(defaultHeapData->defaultDynLoadAlloc,
                             defaultHeapData->defaultDynLoadFree);
}

virt_ptr<void>
MEMAllocFromDefaultHeap(uint32_t size)
{
   auto defaultHeapData = getDefaultHeapData();
   return cafe::invoke(cpu::this_core::state(),
                       defaultHeapData->allocFromDefaultHeap,
                       size);

}

virt_ptr<void>
MEMAllocFromDefaultHeapEx(uint32_t size,
                          int32_t align)
{
   auto defaultHeapData = getDefaultHeapData();
   return cafe::invoke(cpu::this_core::state(),
                       defaultHeapData->allocFromDefaultHeapEx,
                       size, align);

}

void
MEMFreeToDefaultHeap(virt_ptr<void> ptr)
{
   auto defaultHeapData = getDefaultHeapData();
   return cafe::invoke(cpu::this_core::state(),
                       defaultHeapData->freeToDefaultHeap,
                       ptr);

}

void
Library::initialiseDefaultHeapStaticData()
{
   auto defaultHeapData = allocStaticData<StaticDefaultHeapData>();
   Library::getStaticData()->defaultHeapData = defaultHeapData;

   // Register data exports
   RegisterDataExport("MEMAllocFromDefaultHeap", virt_addrof(defaultHeapData->allocFromDefaultHeap));
   RegisterDataExport("MEMAllocFromDefaultHeapEx", virt_addrof(defaultHeapData->allocFromDefaultHeapEx));
   RegisterDataExport("MEMFreeToDefaultHeap", virt_addrof(defaultHeapData->freeToDefaultHeap));

   // Get internal function addresses
   defaultHeapData->defaultAllocFromDefaultHeap = GetInternalFunctionAddress(defaultAllocFromDefaultHeap);
   defaultHeapData->defaultAllocFromDefaultHeapEx = GetInternalFunctionAddress(defaultAllocFromDefaultHeapEx);
   defaultHeapData->defaultFreeToDefaultHeap = GetInternalFunctionAddress(defaultFreeToDefaultHeap);
   defaultHeapData->defaultDynLoadAlloc = GetInternalFunctionAddress(defaultDynLoadAlloc);
   defaultHeapData->defaultDynLoadFree = GetInternalFunctionAddress(defaultDynLoadFree);
}

void
Library::registerMemDefaultHeapFunctions()
{
   RegisterFunctionExport(CoreInitDefaultHeap);
   RegisterFunctionInternal(defaultAllocFromDefaultHeap);
   RegisterFunctionInternal(defaultAllocFromDefaultHeapEx);
   RegisterFunctionInternal(defaultFreeToDefaultHeap);
}

} // namespace cafe::coreinit

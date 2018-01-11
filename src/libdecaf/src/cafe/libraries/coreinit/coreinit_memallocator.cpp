#include "coreinit.h"
#include "coreinit_enum.h"
#include "coreinit_memallocator.h"
#include "coreinit_memblockheap.h"
#include "coreinit_memdefaultheap.h"
#include "coreinit_memexpheap.h"
#include "coreinit_memframeheap.h"
#include "coreinit_memunitheap.h"
#include "cafe/cafe_ppc_interface_invoke.h"

#include <common/decaf_assert.h>
#include <common/log.h>
#include <libcpu/cpu.h>

namespace cafe::coreinit
{

struct StaticAllocatorData
{
   be2_struct<MEMAllocatorFunctions> defaultHeapFunctions;
   be2_struct<MEMAllocatorFunctions> blockHeapFunctions;
   be2_struct<MEMAllocatorFunctions> expHeapFunctions;
   be2_struct<MEMAllocatorFunctions> frameHeapFunctions;
   be2_struct<MEMAllocatorFunctions> unitHeapFunctions;
};

static virt_ptr<StaticAllocatorData>
getAllocatorData()
{
   return Library::getStaticData()->allocatorData;
}


/**
 * Initialise an Allocator struct for the default heap.
 */
void
MEMInitAllocatorForDefaultHeap(virt_ptr<MEMAllocator> allocator)
{
   auto allocatorData = getAllocatorData();
   allocator->heap = MEMGetBaseHeapHandle(MEMBaseHeapType::MEM2);
   allocator->align = 0;
   allocator->funcs = virt_addrof(allocatorData->defaultHeapFunctions);
}


/**
 * Initialise an Allocator struct for a block heap.
 */
void
MEMInitAllocatorForBlockHeap(virt_ptr<MEMAllocator> allocator,
                             MEMHeapHandle handle,
                             int alignment)
{
   auto allocatorData = getAllocatorData();
   allocator->heap = handle;
   allocator->align = alignment;
   allocator->funcs = virt_addrof(allocatorData->blockHeapFunctions);
}


/**
 * Initialise an Allocator struct for an expanded heap.
 */
void
MEMInitAllocatorForExpHeap(virt_ptr<MEMAllocator> allocator,
                           MEMHeapHandle handle,
                           int alignment)
{
   auto allocatorData = getAllocatorData();
   allocator->heap = handle;
   allocator->align = alignment;
   allocator->funcs = virt_addrof(allocatorData->expHeapFunctions);
}


/**
 * Initialise an Allocator struct for a frame heap.
 */
void
MEMInitAllocatorForFrmHeap(virt_ptr<MEMAllocator> allocator,
                           MEMHeapHandle handle,
                           int alignment)
{
   auto allocatorData = getAllocatorData();
   allocator->heap = handle;
   allocator->align = alignment;
   allocator->funcs = virt_addrof(allocatorData->frameHeapFunctions);
}


/**
 * Initialise an Allocator struct for a unit heap.
 */
void
MEMInitAllocatorForUnitHeap(virt_ptr<MEMAllocator> allocator,
                            MEMHeapHandle handle)
{
   auto allocatorData = getAllocatorData();
   allocator->heap = handle;
   allocator->align = 0;
   allocator->funcs = virt_addrof(allocatorData->unitHeapFunctions);
}


/**
 * Allocate memory from an Allocator.
 *
 * \return Returns pointer to new allocated memory.
 */
virt_ptr<void>
MEMAllocFromAllocator(virt_ptr<MEMAllocator> allocator,
                      uint32_t size)
{
   return cafe::invoke(cpu::this_core::state(),
                       allocator->funcs->alloc,
                       allocator, size);
}


/**
 * Free memory from an Allocator.
 */
void
MEMFreeToAllocator(virt_ptr<MEMAllocator> allocator,
                   virt_ptr<void> block)
{
   return cafe::invoke(cpu::this_core::state(),
                       allocator->funcs->free,
                       allocator, block);
}


static virt_ptr<void>
allocatorDefaultHeapAlloc(virt_ptr<MEMAllocator> allocator,
                          uint32_t size)
{
   return MEMAllocFromDefaultHeap(size);
}

static void
allocatorDefaultHeapFree(virt_ptr<MEMAllocator> allocator,
                         virt_ptr<void> block)
{
   MEMFreeToDefaultHeap(block);
}

static virt_ptr<void>
allocatorBlockHeapAlloc(virt_ptr<MEMAllocator> allocator,
                        uint32_t size)
{
   return MEMAllocFromBlockHeapEx(allocator->heap,
                                  size,
                                  allocator->align);
}

static void
allocatorBlockHeapFree(virt_ptr<MEMAllocator> allocator,
                       virt_ptr<void> block)
{
   MEMFreeToBlockHeap(allocator->heap, block);
}

static virt_ptr<void>
allocatorExpHeapAlloc(virt_ptr<MEMAllocator> allocator,
                      uint32_t size)
{
   return MEMAllocFromExpHeapEx(allocator->heap,
                                size,
                                allocator->align);
}

static void
allocatorExpHeapFree(virt_ptr<MEMAllocator> allocator,
                     virt_ptr<void> block)
{
   MEMFreeToExpHeap(allocator->heap, block);
}

static virt_ptr<void>
allocatorFrameHeapAlloc(virt_ptr<MEMAllocator> allocator,
                      uint32_t size)
{
   return MEMAllocFromFrmHeapEx(allocator->heap,
                                size,
                                allocator->align);
}

static void
allocatorFrameHeapFree(virt_ptr<MEMAllocator> allocator,
                     virt_ptr<void> block)
{
   /* Woooowwww I sure hope no one uses frame heap in an allocator...
    *
    * coreinit.rpl does not actually free memory here, probably because
    * using a frame heap for an allocator where you do not know the exact
    * order of alloc and free is a really dumb idea
    */
   gLog->warn("Allocator did not free memory allocated from frame heap");
}

static virt_ptr<void>
allocatorUnitHeapAlloc(virt_ptr<MEMAllocator> allocator,
                       uint32_t size)
{
   return MEMAllocFromUnitHeap(allocator->heap);
}

static void
allocatorUnitHeapFree(virt_ptr<MEMAllocator> allocator,
                      virt_ptr<void> block)
{
   MEMFreeToUnitHeap(allocator->heap, block);
}

void
Library::initialiseAllocatorStaticData()
{
   // TODO: Allocate from static memory frame allocator
   auto allocatorData = virt_ptr<StaticAllocatorData> { nullptr };
   Library::getStaticData()->allocatorData = allocatorData;

   allocatorData->defaultHeapFunctions.alloc = GetInternalFunctionAddress(allocatorDefaultHeapAlloc);
   allocatorData->defaultHeapFunctions.free = GetInternalFunctionAddress(allocatorDefaultHeapFree);
   allocatorData->blockHeapFunctions.alloc = GetInternalFunctionAddress(allocatorBlockHeapAlloc);
   allocatorData->blockHeapFunctions.free = GetInternalFunctionAddress(allocatorBlockHeapFree);
   allocatorData->expHeapFunctions.alloc = GetInternalFunctionAddress(allocatorExpHeapAlloc);
   allocatorData->expHeapFunctions.free = GetInternalFunctionAddress(allocatorExpHeapFree);
   allocatorData->frameHeapFunctions.alloc = GetInternalFunctionAddress(allocatorFrameHeapAlloc);
   allocatorData->frameHeapFunctions.free = GetInternalFunctionAddress(allocatorFrameHeapFree);
   allocatorData->unitHeapFunctions.alloc = GetInternalFunctionAddress(allocatorUnitHeapAlloc);
   allocatorData->unitHeapFunctions.free = GetInternalFunctionAddress(allocatorUnitHeapFree);
}

void
Library::registerAllocatorExports()
{
   RegisterFunctionExport(MEMInitAllocatorForDefaultHeap);
   RegisterFunctionExport(MEMInitAllocatorForBlockHeap);
   RegisterFunctionExport(MEMInitAllocatorForExpHeap);
   RegisterFunctionExport(MEMInitAllocatorForFrmHeap);
   RegisterFunctionExport(MEMInitAllocatorForUnitHeap);
   RegisterFunctionExport(MEMAllocFromAllocator);
   RegisterFunctionExport(MEMFreeToAllocator);

   RegisterFunctionInternal(allocatorDefaultHeapAlloc);
   RegisterFunctionInternal(allocatorDefaultHeapFree);
   RegisterFunctionInternal(allocatorBlockHeapAlloc);
   RegisterFunctionInternal(allocatorBlockHeapFree);
   RegisterFunctionInternal(allocatorExpHeapAlloc);
   RegisterFunctionInternal(allocatorExpHeapFree);
   RegisterFunctionInternal(allocatorFrameHeapAlloc);
   RegisterFunctionInternal(allocatorFrameHeapFree);
   RegisterFunctionInternal(allocatorUnitHeapAlloc);
   RegisterFunctionInternal(allocatorUnitHeapFree);
}

} // namespace cafe::coreinit

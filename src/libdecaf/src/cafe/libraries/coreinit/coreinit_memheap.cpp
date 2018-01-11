#include "coreinit.h"
#include "coreinit_memexpheap.h"
#include "coreinit_memheap.h"
#include "coreinit_memlist.h"
#include "coreinit_memory.h"
#include "coreinit_memunitheap.h"
#include "coreinit_spinlock.h"
#include "cafe/cafe_stackobject.h"

#include <algorithm>
#include <array>

namespace cafe::coreinit
{

struct StaticMemHeapData
{
   be2_array<MEMHeapHandle, MEMBaseHeapType::Max> arenas;
   be2_struct<MEMList> foregroundList;
   be2_struct<MEMList> mem1List;
   be2_struct<MEMList> mem2List;
   be2_struct<OSSpinLock> lock;
   be2_array<uint32_t, MEMHeapFillType::Max> fillValues;
};

static virt_ptr<StaticMemHeapData>
getMemHeapData()
{
   return Library::getStaticData()->memHeapData;
}

static virt_ptr<MEMList>
findListContainingHeap(virt_ptr<MEMHeapHeader> heap)
{
   auto memHeapData = getMemHeapData();
   StackObject<uint32_t> start, size, end;
   OSGetForegroundBucket(start, size);
   end = start + size;

   if (heap->dataStart >= start && heap->dataEnd <= end) {
      return memHeapData->foregroundList;
   } else {
      OSGetMemBound(OSMemoryType::MEM1, start, size);
      end = start + size;

      if (heap->dataStart >= start && heap->dataEnd <= end) {
         return memHeapData->mem1List;
      } else {
         return memHeapData->mem2List;
      }
   }
}

static virt_ptr<MEMList>
findListContainingBlock(virt_ptr<void> block)
{
   auto memHeapData = getMemHeapData();
   StackObject<uint32_t> start, size, end;
   OSGetForegroundBucket(&start, &size);
   end = start + size;

   if (block >= start && block <= end) {
      return memHeapData->foregroundList;
   } else {
      OSGetMemBound(OSMemoryType::MEM1, &start, &size);
      end = start + size;

      if (block >= start && block <= end) {
         return memHeapData->mem1List;
      } else {
         return memHeapData->mem2List;
      }
   }
}

static virt_ptr<MEMHeapHeader>
findHeapContainingBlock(virt_ptr<MEMList> list,
                        virt_ptr<void> block)
{
   virt_ptr<MEMHeapHeader> heap = nullptr;

   while ((heap = virt_cast<MEMHeapHeader *>(MEMGetNextListObject(list, heap)))) {
      if (block >= heap->dataStart && block < heap->dataEnd) {
         auto child = findHeapContainingBlock(&heap->list, block);
         return child ? child : heap;
      }
   }

   return nullptr;
}

void
MEMDumpHeap(virt_ptr<MEMHeapHeader> heap)
{
   switch (heap->tag) {
   case MEMHeapTag::ExpandedHeap:
      internal::dumpExpandedHeap(virt_cast<MEMExpHeap *>(heap));
      break;
   case MEMHeapTag::UnitHeap:
      internal::dumpUnitHeap(virt_cast<MEMUnitHeap *>(heap));
      break;
   case MEMHeapTag::FrameHeap:
   case MEMHeapTag::UserHeap:
   case MEMHeapTag::BlockHeap:
      gLog->info("Unimplemented MEMDumpHeap type");
   }
}

virt_ptr<MEMHeapHeader>
MEMFindContainHeap(virt_ptr<void> block)
{
   if (auto list = findListContainingBlock(block)) {
      return findHeapContainingBlock(list, block);
   }

   return nullptr;
}

MEMBaseHeapType
MEMGetArena(virt_ptr<MEMHeapHeader> heap)
{
   auto memHeapData = getMemHeapData();

   for (auto i = 0u; i < memHeapData->arenas.size(); ++i) {
      if (memHeapData->arenas[i] == heap) {
         return static_cast<MEMBaseHeapType>(i);
      }
   }

   return MEMBaseHeapType::Invalid;
}

MEMHeapHandle
MEMGetBaseHeapHandle(MEMBaseHeapType type)
{
   auto memHeapData = getMemHeapData();

   if (type < memHeapData->arenas.size()) {
      return memHeapData->arenas[type];
   } else {
      return nullptr;
   }
}

MEMHeapHandle
MEMSetBaseHeapHandle(MEMBaseHeapType type,
                     MEMHeapHandle heap)
{
   auto memHeapData = getMemHeapData();

   if (type < memHeapData->arenas.size()) {
      auto previous = memHeapData->arenas[type];
      memHeapData->arenas[type] = heap;
      return previous;
   } else {
      return nullptr;
   }
}

MEMHeapHandle
MEMCreateUserHeapHandle(virt_ptr<MEMHeapHeader> heap,
                        uint32_t size)
{
   auto dataStart = virt_cast<uint8_t *>(heap) + sizeof(MEMHeapHeader);
   auto dataEnd = dataStart + size;

   internal::registerHeap(heap,
                          coreinit::MEMHeapTag::UserHeap,
                          dataStart,
                          dataEnd,
                          0);

   return heap;
}

uint32_t
MEMGetFillValForHeap(MEMHeapFillType type)
{
   return getMemHeapData()->fillValues[type];
}

void
MEMSetFillValForHeap(MEMHeapFillType type,
                     uint32_t value)
{
   getMemHeapData()->fillValues[type] = value;
}

void
Library::registerMemHeapFunctions()
{
   RegisterFunctionExport(MEMGetBaseHeapHandle);
   RegisterFunctionExport(MEMSetBaseHeapHandle);
   RegisterFunctionExport(MEMCreateUserHeapHandle);
   RegisterFunctionExport(MEMGetArena);
   RegisterFunctionExport(MEMFindContainHeap);
   RegisterFunctionExport(MEMDumpHeap);
   RegisterFunctionExport(MEMGetFillValForHeap);
   RegisterFunctionExport(MEMSetFillValForHeap);
}

void
Library::initialiseMemHeapStaticData()
{
   // TODO: Alloc static data
   auto memHeapData = nullptr;
   Library::getStaticData()->memHeapData = memHeapData;

   OSInitSpinLock(virt_addrof(memHeapData->lock));
   MEMInitList(virt_addrof(memHeapData->foregroundList), offsetof(MEMHeapHeader, link));
   MEMInitList(virt_addrof(memHeapData->mem1List), offsetof(MEMHeapHeader, link));
   MEMInitList(virt_addrof(memHeapData->mem2List), offsetof(MEMHeapHeader, link));

   memHeapData->arenas.fill(nullptr);
   memHeapData->fillValues[0] = 0xC3C3C3C3u;
   memHeapData->fillValues[1] = 0xF3F3F3F3u;
   memHeapData->fillValues[2] = 0xD3D3D3D3u;
}

namespace internal
{

void
registerHeap(virt_ptr<MEMHeapHeader> heap,
             MEMHeapTag tag,
             virt_ptr<uint8_t> dataStart,
             virt_ptr<uint8_t> dataEnd,
             uint32_t flags)
{
   auto memHeapData = getMemHeapData();

   // Setup heap header
   heap->tag = tag;
   heap->dataStart = dataStart;
   heap->dataEnd = dataEnd;
   heap->attribs = MEMHeapAttribs::get(flags);

   if (heap->attribs.value().debugMode()) {
      auto fillVal = MEMGetFillValForHeap(MEMHeapFillType::Unused);
      std::memset(dataStart, fillVal, dataEnd - dataStart);
   }

   MEMInitList(virt_addrof(heap->list), offsetof(MEMHeapHeader, link));

   OSInitSpinLock(virt_addrof(heap->lock));

   // Add to heap list
   OSUninterruptibleSpinLock_Acquire(virt_addrof(memHeapData->lock));

   if (auto list = findListContainingHeap(heap)) {
      MEMAppendListObject(list, heap);
   }

   OSUninterruptibleSpinLock_Release(virt_addrof(memHeapData->lock));
}

void
unregisterHeap(virt_ptr<MEMHeapHeader> heap)
{
   auto memHeapData = getMemHeapData();
   OSUninterruptibleSpinLock_Acquire(virt_addrof(memHeapData->lock));

   if (auto list = findListContainingHeap(heap)) {
      MEMRemoveListObject(list, heap);
   }

   OSUninterruptibleSpinLock_Release(virt_addrof(memHeapData->lock));
}

} // namespace internal

} // namespace cafe::coreinit

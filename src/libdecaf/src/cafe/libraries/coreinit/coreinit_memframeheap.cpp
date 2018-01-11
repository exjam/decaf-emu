#include "coreinit.h"
#include "coreinit_memheap.h"
#include "coreinit_memframeheap.h"
#include "coreinit_memory.h"

namespace cafe::coreinit
{

MEMHeapHandle
MEMCreateFrmHeapEx(virt_ptr<void> base,
                   uint32_t size,
                   uint32_t flags)
{
   decaf_check(base);
   auto baseMem = virt_cast<uint8_t *>(base);

   // Align start and end to 4 byte boundary
   auto start = align_up(baseMem, 4);
   auto end = align_down(baseMem + size, 4);

   if (start >= end) {
      return nullptr;
   }

   if (end - start < sizeof(MEMFrameHeap)) {
      return nullptr;
   }

   // Setup the frame heap
   auto heap = virt_cast<MEMFrameHeap *>(start);

   internal::registerHeap(virt_addrof(heap->header),
                          MEMHeapTag::FrameHeap,
                          start + sizeof(MEMFrameHeap),
                          end,
                          flags);

   heap->head = heap->header.dataStart;
   heap->tail = heap->header.dataEnd;
   heap->previousState = nullptr;
   return heap;
}

virt_ptr<void>
MEMDestroyFrmHeap(MEMHeapHandle handle)
{
   auto heap = virt_cast<MEMFrameHeap *>(handle);
   decaf_check(heap);
   decaf_check(heap->header.tag == MEMHeapTag::FrameHeap);
   internal::unregisterHeap(virt_addrof(heap->header));
   return heap;
}

virt_ptr<void>
MEMAllocFromFrmHeapEx(MEMHeapHandle handle,
                      uint32_t size,
                      int alignment)
{
   auto heap = virt_cast<MEMFrameHeap *>(handle);
   decaf_check(heap);
   decaf_check(heap->header.tag == MEMHeapTag::FrameHeap);

   // Yes coreinit.rpl actually does this
   if (size == 0) {
      size = 1;
   }

   internal::HeapLock lock { virt_addrof(heap->header) };
   auto heapAttribs = heap->header.attribs.value();
   auto block = virt_ptr<void> { nullptr };

   if (alignment < 0) {
      // Allocate from bottom
      auto tail = align_down(heap->tail.get() - size, -alignment);

      if (tail < heap->head) {
         // Not enough space!
         return nullptr;
      }

      heap->tail = tail;
      block = tail;
   } else {
      // Allocate from head
      auto addr = align_up(heap->head.get(), alignment);
      auto head = addr + size;

      if (head > heap->tail) {
         // Not enough space!
         return nullptr;
      }

      heap->head = head;
      block = addr;
   }

   lock.unlock();

   if (heapAttribs.zeroAllocated()) {
      memset(block, 0, size);
   } else if (heapAttribs.debugMode()) {
      auto value = MEMGetFillValForHeap(MEMHeapFillType::Allocated);
      memset(block, value, size);
   }

   return block;
}


void
MEMFreeToFrmHeap(MEMHeapHandle handle,
                 MEMFrameHeapFreeMode mode)
{
   auto heap = virt_cast<MEMFrameHeap *>(handle);
   decaf_check(heap);
   decaf_check(heap->header.tag == MEMHeapTag::FrameHeap);

   internal::HeapLock lock { virt_addrof(heap->header) };
   auto heapAttribs = heap->header.attribs.value();

   if (mode & MEMFrameHeapFreeMode::Head) {
      if (heapAttribs.debugMode()) {
         auto value = MEMGetFillValForHeap(MEMHeapFillType::Freed);
         memset(heap->header.dataStart, value, heap->head.get() - heap->header.dataStart);
      }

      heap->head = heap->header.dataStart;
      heap->previousState = nullptr;
   }

   if (mode & MEMFrameHeapFreeMode::Tail) {
      if (heapAttribs.debugMode()) {
         auto value = MEMGetFillValForHeap(MEMHeapFillType::Freed);
         memset(heap->tail, value, heap->header.dataEnd.get() - heap->tail);
      }

      heap->tail = heap->header.dataEnd;
      heap->previousState = nullptr;
   }
}

BOOL
MEMRecordStateForFrmHeap(MEMHeapHandle handle,
                         uint32_t tag)
{
   auto heap = virt_cast<MEMFrameHeap *>(handle);
   decaf_check(heap);
   decaf_check(heap->header.tag == MEMHeapTag::FrameHeap);

   internal::HeapLock lock { virt_addrof(heap->header) };
   auto heapAttribs = heap->header.attribs.value();
   auto state = virt_cast<MEMFrameHeapState *>(MEMAllocFromFrmHeapEx(heap, sizeof(MEMFrameHeapState), 4));
   auto result = FALSE;

   if (state) {
      state->tag = tag;
      state->head = heap->head;
      state->tail = heap->tail;
      state->previous = heap->previousState;
      heap->previousState = state;

      result = TRUE;
   }

   return result;
}

BOOL
MEMFreeByStateToFrmHeap(MEMHeapHandle handle,
                        uint32_t tag)
{
   auto heap = virt_cast<MEMFrameHeap *>(handle);
   decaf_check(heap);
   decaf_check(heap->header.tag == MEMHeapTag::FrameHeap);

   internal::HeapLock lock { virt_addrof(heap->header) };
   auto result = FALSE;
   auto heapAttribs = heap->header.attribs.value();

   // Find the state to reset to
   auto state = heap->previousState;

   if (tag != 0) {
      while (state) {
         if (state->tag == tag) {
            break;
         }

         state = state->previous;
      }
   }

   // Reset to state
   if (state) {
      if (heapAttribs.debugMode()) {
         auto value = MEMGetFillValForHeap(MEMHeapFillType::Freed);
         memset(state->head, value, heap->head.get() - state->head);
         memset(heap->tail, value, state->tail.get() - heap->tail);
      }

      heap->head = state->head;
      heap->tail = state->tail;
      heap->previousState = state->previous;
      result = TRUE;
   }

   return result;
}

uint32_t
MEMAdjustFrmHeap(MEMHeapHandle handle)
{
   auto heap = virt_cast<MEMFrameHeap *>(handle);
   decaf_check(heap);
   decaf_check(heap->header.tag == MEMHeapTag::FrameHeap);

   internal::HeapLock lock { virt_addrof(heap->header) };
   auto result = 0u;

   // We can only adjust the heap if we have no tail allocated memory
   if (heap->tail == heap->header.dataEnd) {
      heap->header.dataEnd = heap->head;
      heap->tail = heap->head;

      auto heapMemStart = reinterpret_cast<uint8_t *>(heap);
      result = static_cast<uint32_t>(heap->header.dataEnd.get() - heapMemStart);
   }

   return result;
}

uint32_t
MEMResizeForMBlockFrmHeap(MEMHeapHandle handle,
                          virt_ptr<void> address,
                          uint32_t size)
{
   auto heap = virt_cast<MEMFrameHeap *>(handle);
   decaf_check(heap);
   decaf_check(heap->header.tag == MEMHeapTag::FrameHeap);

   internal::HeapLock lock { virt_addrof(heap->header) };
   auto heapAttribs = static_cast<MEMHeapAttribs>(heap->header.attribs);
   auto result = 0u;

   decaf_check(address > heap->head);
   decaf_check(address < heap->tail);
   decaf_check(heap->previousState == nullptr || heap->previousState < address);

   if (size == 0) {
      size = 1;
   }

   auto addrMem = reinterpret_cast<uint8_t *>(address);
   auto end = align_up(addrMem + size, 4);

   if (end > heap->tail) {
      // Not enough free space
      result = 0;
   } else if (end == heap->head) {
      // Same size
      result = size;
   } else if (end < heap->head) {
      // Decrease size
      if (heapAttribs.debugMode()) {
         auto value = MEMGetFillValForHeap(MEMHeapFillType::Freed);
         std::memset(end, value, heap->head.get() - addrMem);
      }

      heap->head = end;
      result = size;
   } else if (end > heap->head) {
      // Increase size
      if (heapAttribs.zeroAllocated()) {
         std::memset(heap->head, 0, addrMem - heap->head);
      } else if (heapAttribs.debugMode()) {
         auto value = MEMGetFillValForHeap(MEMHeapFillType::Allocated);
         std::memset(heap->head, value, addrMem - heap->head);
      }

      heap->head = end;
      result = size;
   }

   return result;
}

uint32_t
MEMGetAllocatableSizeForFrmHeapEx(MEMHeapHandle handle,
                                  int alignment)
{
   auto heap = virt_cast<MEMFrameHeap *>(handle);
   decaf_check(heap);
   decaf_check(heap->header.tag == MEMHeapTag::FrameHeap);

   internal::HeapLock lock { virt_addrof(heap->header) };
   auto alignedHead = align_up(heap->head.get(), alignment);
   auto result = 0u;

   if (alignedHead < heap->tail) {
      result = static_cast<uint32_t>(heap->tail.get() - alignedHead);
   }

   return result;
}

void
Library::registerMemFrameHeapFunctions()
{
   RegisterKernelFunction(MEMCreateFrmHeapEx);
   RegisterKernelFunction(MEMDestroyFrmHeap);
   RegisterKernelFunction(MEMAllocFromFrmHeapEx);
   RegisterKernelFunction(MEMFreeToFrmHeap);
   RegisterKernelFunction(MEMRecordStateForFrmHeap);
   RegisterKernelFunction(MEMFreeByStateToFrmHeap);
   RegisterKernelFunction(MEMAdjustFrmHeap);
   RegisterKernelFunction(MEMResizeForMBlockFrmHeap);
   RegisterKernelFunction(MEMGetAllocatableSizeForFrmHeapEx);
}

} // namespace cafe::coreinit

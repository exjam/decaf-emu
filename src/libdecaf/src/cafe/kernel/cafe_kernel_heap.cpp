#include "cafe_kernel_heap.h"
#include "cafe_kernel_mmu.h"

#include <common/teenyheap.h>

namespace cafe::kernel::internal
{

static TeenyHeap
sKernelHeap;

void
initialiseHeap()
{
   auto heapRange = getKernelVirtualAddressRange(VirtualRegion::KernelHeap);
   sKernelHeap.reset(virt_cast<void *>(heapRange.start).getRawPointer(),
                     heapRange.size);
}

} // namespace cafe::kernel::internal

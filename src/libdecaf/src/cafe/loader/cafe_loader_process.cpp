#include "cafe_loader_process.h"

#include <libcpu/be2_struct.h>
#include <common/frameallocator.h>

namespace cafe::loader::internal
{

static FrameAllocator
sStaticHeapAllocator;

void
initialiseProcessStaticHeap()
{
   // TODO: Get this region from kernel!!
   sStaticHeapAllocator = FrameAllocator {
      virt_cast<void *>(virt_addr{ 0xEFE00000 }).getRawPointer(),
      0x80000
   };
}

virt_ptr<void>
allocProcessStatic(uint32_t size,
                   uint32_t align)
{
   auto buffer = sStaticHeapAllocator.allocate(size, align < 4 ? 4 : align);
   std::memset(buffer, 0, size);
   return virt_cast<void *>(cpu::translate(buffer));
}

} // namespace cafe::loader::internal

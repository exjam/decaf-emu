#include "cafe_kernel_ios.h"
#include <common/teenyheap.h>

namespace cafe::kernel::internal
{

constexpr auto IpcBufferSize = 0x4000u;

static TeenyHeap
gIpcHeap;

void
initIpcBuffer()
{
   void *buffer = nullptr;
   gIpcHeap.reset(buffer, IpcBufferSize);
}

virt_ptr<void>
allocIpcBuffer(uint32_t size)
{
   return virt_cast<void *>(cpu::translate(gIpcHeap.alloc(size, 0x40u)));
}

void
freeIpcBuffer(virt_ptr<void> buffer)
{
}

} // namespace cafe::kernel::internal

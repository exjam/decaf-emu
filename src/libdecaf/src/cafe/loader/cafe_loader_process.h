#pragma once
#include <libcpu/be2_struct.h>

namespace cafe::loader::internal
{

void
initialiseProcessStaticHeap();

virt_ptr<void>
allocProcessStatic(uint32_t size,
                   uint32_t align = 4u);

} // namespace cafe::loader::internal

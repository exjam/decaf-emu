#pragma once
#include "ios/ios_ipc.h"

namespace cafe::kernel::internal
{

void
initIpcBuffer();

virt_ptr<void>
allocIpcBuffer(uint32_t size);

void
freeIpcBuffer(virt_ptr<void> buffer);

} // namespace cafe::kernel::internal

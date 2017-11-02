#pragma once
#include "ios/ios_ipc.h"
#include "cafe_kernel_ipckdriver.h"

namespace cafe::kernel::internal
{

ios::Error
IOS_OpenAsync(RamProcessId clientProcessId,
              virt_ptr<const char> device,
              ios::OpenMode mode,
              IPCKDriverAsyncCallbackFn asyncCallback,
              virt_ptr<void> asyncContext);

ios::Error
IOS_Open(RamProcessId clientProcessId,
         virt_ptr<const char> device,
         ios::OpenMode mode);

ios::Error
IOS_CloseAsync(RamProcessId clientProcessId,
               ios::Handle handle,
               IPCKDriverAsyncCallbackFn asyncCallback,
               virt_ptr<void> asyncContext,
               uint32_t unkArg0);

ios::Error
IOS_Close(RamProcessId clientProcessId,
          ios::Handle handle,
          uint32_t unkArg0);

virt_ptr<void>
allocIpcBuffer(uint32_t size);

void
freeIpcBuffer(virt_ptr<void> buffer);

void
initialiseIpc();

void
initialiseStaticIpcData();

} // namespace cafe::kernel::internal

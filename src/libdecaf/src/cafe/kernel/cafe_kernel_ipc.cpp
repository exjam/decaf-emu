#include "cafe_kernel.h"
#include "cafe_kernel_ipc.h"
#include "cafe_kernel_ipckdriver.h"
#include "cafe_kernel_mmu.h"
#include "cafe/cafe_stackobject.h"
#include "ios/ios_ipc.h"

#include <common/teenyheap.h>
#include <libcpu/cpu.h>

namespace cafe::kernel::internal
{

constexpr auto IpcBufferSize = 0x4000u;
constexpr auto IpcBufferAlign = 0x40u;

struct StaticIpcData
{
   be2_array<std::byte, IpcBufferSize> ipcHeapBuffer;
   be2_val<int32_t> mcpHandle;
   be2_val<int32_t> ppcAppHandle;
   be2_val<int32_t> cblHandle;
};

struct SynchronousCallback
{
   std::atomic<bool> replyReceived = false;
   ios::Error error = ios::Error::InvalidArg;
   virt_ptr<void> buffer = nullptr;
};

static virt_ptr<StaticIpcData>
sIpcData;

static TeenyHeap
gIpcHeap;

static void
initialiseIpcHeap()
{
   gIpcHeap.reset(virt_addrof(sIpcData->ipcHeapBuffer[0]).getRawPointer(),
                  IpcBufferSize);
}

virt_ptr<void>
allocIpcBuffer(uint32_t size)
{
   auto buffer = gIpcHeap.alloc(align_up(size, IpcBufferAlign), IpcBufferAlign);
   return virt_cast<void *>(cpu::translate(buffer));
}

void
freeIpcBuffer(virt_ptr<void> buffer)
{
   gIpcHeap.free(buffer.getRawPointer());
}

static void
synchronousCallback(ios::Error error,
                    virt_ptr<void> context)
{
   auto synchronousReply = virt_cast<SynchronousCallback *>(context);
   synchronousReply->error = error;
   synchronousReply->replyReceived = true;

   if (synchronousReply->buffer) {
      freeIpcBuffer(synchronousReply->buffer);
   }
}

static ios::Error
waitSynchronousReply(virt_ptr<SynchronousCallback> synchronousReply,
                     std::chrono::microseconds timeout,
                     uint32_t unk)
{
   auto waitUntil = std::chrono::steady_clock::now() + timeout;
   auto error = ios::Error::Timeout;

   while (!synchronousReply->replyReceived) {
      cpu::this_core::waitNextInterrupt(waitUntil);

      if (std::chrono::steady_clock::now() >= waitUntil) {
         break;
      }
   }

   if (synchronousReply->replyReceived) {
      error = synchronousReply->error;
      synchronousReply->replyReceived = false;
      synchronousReply->buffer = nullptr;
   }

   return error;
}

ios::Error
IOS_OpenAsync(RamProcessId clientProcessId,
              virt_ptr<const char> device,
              ios::OpenMode mode,
              IPCKDriverAsyncCallbackFn asyncCallback,
              virt_ptr<void> asyncContext)
{
   virt_ptr<IPCKDriverRequestBlock> requestBlock;
   auto driver = IPCKDriver_GetInstance();
   auto error = IPCKDriver_AllocateRequestBlock(clientProcessId,
                                                RamProcessId::Invalid,
                                                driver,
                                                &requestBlock,
                                                0,
                                                ios::Command::Open,
                                                asyncCallback,
                                                asyncContext);
   if (error < ios::Error::OK) {
      return error;
   }

   requestBlock->request->request.args.open.name = virtualToPhysical(device);
   requestBlock->request->request.args.open.nameLen = strlen(device.getRawPointer());
   requestBlock->request->request.args.open.mode = mode;
   requestBlock->request->request.args.open.caps = 0;

   error = IPCKDriver_SubmitRequest(driver, requestBlock);
   if (error < ios::Error::OK) {
      IPCKDriver_FreeRequestBlock(driver, requestBlock);
   }

   return error;
}

ios::Error
IOS_Open(RamProcessId clientProcessId,
         virt_ptr<const char> device,
         ios::OpenMode mode)
{
   StackObject<SynchronousCallback> synchronousReply;
   auto error = IOS_OpenAsync(clientProcessId,
                              device,
                              mode,
                              &synchronousCallback,
                              synchronousReply);
   if (error < ios::Error::OK) {
      return error;
   }

   return waitSynchronousReply(synchronousReply, std::chrono::milliseconds { 35 }, 6);
}

ios::Error
IOS_CloseAsync(RamProcessId clientProcessId,
               ios::Handle handle,
               IPCKDriverAsyncCallbackFn asyncCallback,
               virt_ptr<void> asyncContext,
               uint32_t unkArg0)
{
   virt_ptr<IPCKDriverRequestBlock> requestBlock;
   auto driver = IPCKDriver_GetInstance();
   auto error = IPCKDriver_AllocateRequestBlock(clientProcessId,
                                                RamProcessId::Invalid,
                                                driver,
                                                &requestBlock,
                                                handle,
                                                ios::Command::Open,
                                                asyncCallback,
                                                asyncContext);
   if (error < ios::Error::OK) {
      return error;
   }

   requestBlock->request->request.args.close.unkArg0 = unkArg0;

   error = IPCKDriver_SubmitRequest(driver, requestBlock);
   if (error < ios::Error::OK) {
      IPCKDriver_FreeRequestBlock(driver, requestBlock);
   }

   return error;
}

ios::Error
IOS_Close(RamProcessId clientProcessId,
          ios::Handle handle,
          uint32_t unkArg0)
{
   StackObject<SynchronousCallback> synchronousReply;
   auto error = IOS_CloseAsync(clientProcessId,
                               handle,
                               &synchronousCallback,
                               synchronousReply,
                               unkArg0);
   if (error < ios::Error::OK) {
      return error;
   }

   return waitSynchronousReply(synchronousReply, std::chrono::milliseconds { 35 }, 6);
}

void
initialiseIpc()
{
   initialiseIpcHeap();
   auto nameBuffer = virt_cast<char *>(allocIpcBuffer(0x20));

   std::strncpy(nameBuffer.getRawPointer(), "/dev/mcp", 0x20);
   sIpcData->mcpHandle = IOS_Open(RamProcessId::Kernel, nameBuffer, ios::OpenMode::None);

   std::strncpy(nameBuffer.getRawPointer(), "/dev/ppc_app", 0x20);
   sIpcData->ppcAppHandle = IOS_Open(RamProcessId::Kernel, nameBuffer, ios::OpenMode::None);

   std::strncpy(nameBuffer.getRawPointer(), "/dev/cbl", 0x20);
   sIpcData->cblHandle = IOS_Open(RamProcessId::Kernel, nameBuffer, ios::OpenMode::None);

   freeIpcBuffer(nameBuffer);
}

void
initialiseStaticIpcData()
{
   sIpcData = virt_cast<StaticIpcData *>(allocStatic(sizeof(StaticIpcData), alignof(StaticIpcData)));
}

} // namespace cafe::kernel::internal

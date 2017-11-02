#include "cafe_kernel.h"
#include "cafe_kernel_exception.h"
#include "cafe_kernel_ipckdriver.h"
#include "cafe_kernel_mmu.h"
#include "cafe_kernel_process.h"
#include "ios/ios_enum.h"
#include "ios/kernel/ios_kernel_ipc_thread.h"

#include <common/atomicqueue.h>
#include <libcpu/cpu.h>
#include <libcpu/be2_struct.h>

namespace cafe::kernel
{

static SingleAtomicQueue<phys_ptr<ios::IpcRequest>, 32>
sIpcReplyQueue[3];

void
submitIpcReply(phys_ptr<ios::IpcRequest> reply)
{
   switch (reply->cpuId) {
   case ios::CpuId::PPC0:
      decaf_check(sIpcReplyQueue[0].push(reply));
      cpu::interrupt(0, cpu::IPC_INTERRUPT);
      break;
   case ios::CpuId::PPC1:
      decaf_check(sIpcReplyQueue[1].push(reply));
      cpu::interrupt(1, cpu::IPC_INTERRUPT);
      break;
   case ios::CpuId::PPC2:
      decaf_check(sIpcReplyQueue[2].push(reply));
      cpu::interrupt(2, cpu::IPC_INTERRUPT);
      break;
   }
}

void
handleIpcInterrupt()
{
   phys_ptr<ios::IpcRequest> reply;
   auto coreId = cpu::this_core::id();

   decaf_check(sIpcReplyQueue[coreId].pop(reply));
   internal::IPCKDriver_ProcessReply(internal::IPCKDriver_GetInstance(), physicalToVirtual(reply));
}

ios::Error
userOpenIpckDriver(uint32_t numReplies,
                   virt_ptr<IPCKDriverReplyQueue> replyQueue,
                   UserExceptionCallbackFn callback,
                   virt_ptr<void> callbackStack,
                   virt_ptr<Context> callbackContext)
{
   return internal::IPCKDriver_UserOpen(internal::IPCKDriver_GetInstance(),
                                        numReplies,
                                        replyQueue,
                                        callback,
                                        callbackStack,
                                        callbackContext);
}

ios::Error
userCloseIpckDriver()
{
   return internal::IPCKDriver_UserClose(internal::IPCKDriver_GetInstance());
}

ios::Error
userSubmitIpckRequest(virt_ptr<IPCKDriverRequest> request)
{
   return internal::IPCKDriver_SubmitLoaderOrUserRequest(internal::IPCKDriver_GetInstance(),
                                                         request,
                                                         false);
}

ios::Error
loaderOpenIpckDriver()
{
   return internal::IPCKDriver_LoaderOpen(internal::IPCKDriver_GetInstance());
}

ios::Error
loaderSubmitIpckRequest(virt_ptr<IPCKDriverRequest> request)
{
   return internal::IPCKDriver_SubmitLoaderOrUserRequest(internal::IPCKDriver_GetInstance(),
                                                         request,
                                                         true);
}

virt_ptr<IPCKDriverRequest>
loaderPollIpckCompletion()
{
   return internal::IPCKDriver_LoaderPollCompletion(internal::IPCKDriver_GetInstance());
}

namespace internal
{

struct StaticIpckData
{
   be2_array<IPCKDriver, 3> drivers;
   be2_array<IPCKDriverRequest, IPCKRequestsPerCore * 3> requestBuffer;
};

static virt_ptr<StaticIpckData>
sIpckData;

template<std::size_t Size>
static void
IPCKDriver_FIFOInit(virt_ptr<IPCKDriverFIFO<Size>> fifo)
{
   fifo->pushIndex = 0;
   fifo->popIndex = -1;
   fifo->count = 0;
   fifo->requestBlocks.fill(nullptr);
}

/**
 * Push a request into an IPCKDriverFIFO structure
 *
 * \retval ios::Error::OK
 * Success.
 *
 * \retval ios::Error::QFull
 * There was no free space in the queue to push the request.
 */
template<std::size_t Size>
static ios::Error
IPCKDriver_FIFOPush(virt_ptr<IPCKDriverFIFO<Size>> fifo,
                    virt_ptr<IPCKDriverRequestBlock> requestBlock)
{
   if (fifo->pushIndex == fifo->popIndex) {
      return ios::Error::QFull;
   }

   fifo->requestBlocks[fifo->pushIndex] = requestBlock;

   if (fifo->popIndex == -1) {
      fifo->popIndex = fifo->pushIndex;
   }

   fifo->count += 1;
   fifo->pushIndex = static_cast<int32_t>((fifo->pushIndex + 1) % Size);

   if (fifo->count > fifo->maxCount) {
      fifo->maxCount = fifo->count;
   }

   return ios::Error::OK;
}

/**
 * Pop a request into an IPCKDriverFIFO structure.
 *
 * \retval ios::Error::OK
 * Success.
 *
 * \retval ios::Error::QEmpty
 * There was no requests to pop from the queue.
 */
template<std::size_t Size>
static ios::Error
IPCKDriver_FIFOPop(virt_ptr<IPCKDriverFIFO<Size>> fifo,
                   virt_ptr<IPCKDriverRequestBlock> *outRequestBlock)
{
   if (fifo->popIndex == -1) {
      return ios::Error::QEmpty;
   }

   auto requestBlock = fifo->requestBlocks[fifo->popIndex];
   fifo->count -= 1;

   if (fifo->count == 0) {
      fifo->popIndex = -1;
   } else {
      fifo->popIndex = static_cast<int32_t>((fifo->popIndex + 1) % Size);
   }

   *outRequestBlock = requestBlock;
   return ios::Error::OK;
}

virt_ptr<IPCKDriver>
IPCKDriver_GetInstance()
{
   return virt_addrof(sIpckData->drivers[cpu::this_core::id()]);
}

ios::Error
IPCKDriver_Init(virt_ptr<IPCKDriver> driver)
{
   switch (driver->coreId) {
   case 0:
      driver->registers.ppcMsg = virt_cast<void *>(virt_addr { 0x0D800400 });
      driver->registers.ppcCtrl = virt_cast<void *>(virt_addr { 0x0D800404 });
      driver->registers.armMsg = virt_cast<void *>(virt_addr { 0x0D800408 });
      driver->registers.ahbLt = virt_cast<void *>(virt_addr { 0x0D800444 });
      driver->registers.unkMaybeFlags = 0x40000000u;
      break;
   case 1:
      driver->registers.ppcMsg = virt_cast<void *>(virt_addr { 0x0D800410 });
      driver->registers.ppcCtrl = virt_cast<void *>(virt_addr { 0x0D800414 });
      driver->registers.armMsg = virt_cast<void *>(virt_addr { 0x0D800418 });
      driver->registers.ahbLt = virt_cast<void *>(virt_addr { 0x0D800454 });
      driver->registers.unkMaybeFlags = 0x10000000u;
      break;
   case 2:
      driver->registers.ppcMsg = virt_cast<void *>(virt_addr { 0x0D800420 });
      driver->registers.ppcCtrl = virt_cast<void *>(virt_addr { 0x0D800424 });
      driver->registers.armMsg = virt_cast<void *>(virt_addr { 0x0D800428 });
      driver->registers.ahbLt = virt_cast<void *>(virt_addr { 0x0D800464 });
      driver->registers.unkMaybeFlags = 0x04000000u;
      break;
   }

   return ios::Error::OK;
}

ios::Error
IPCKDriver_Open(virt_ptr<IPCKDriver> driver)
{
   for (auto i = 0u; i < IPCKRequestsPerCore; ++i) {
#ifdef DECAF_KERNEL_LLE
      driver->requestBlocks[i].asyncCallback = nullptr;
#else
      // Allocate space to hold our std::function from guest memory
      auto hostCallbackPtr = virt_cast<IPCKDriverAsyncCallbackFn *>(internal::allocStatic(sizeof(IPCKDriverAsyncCallbackFn),
                                                                                          alignof(IPCKDriverAsyncCallbackFn)));
      new (hostCallbackPtr.getRawPointer()) IPCKDriverAsyncCallbackFn();
      driver->requestBlocks[i].asyncCallback = hostCallbackPtr;
#endif
      driver->requestBlocks[i].asyncContext = nullptr;
      driver->requestBlocks[i].userRequest = nullptr;
      driver->requestBlocks[i].request = virt_addrof(driver->requestsBuffer[i]);
   }

   return ios::Error::OK;
}

ios::Error
IPCKDriver_CloseAll(virt_ptr<IPCKDriver> driver,
                    RamProcessId rampid,
                    uint32_t closeArg0)
{
   // TODO: Close all open IOS devices belonging to rampid
}

ios::Error
IPCKDriver_UserOpen(virt_ptr<IPCKDriver> driver,
                    uint32_t numReplies,
                    virt_ptr<IPCKDriverReplyQueue> replyQueue,
                    UserExceptionCallbackFn callback,
                    virt_ptr<void> callbackStack,
                    virt_ptr<Context> callbackContext)
{
   auto rampid = getRampid();
   auto pidx = static_cast<uint32_t>(getRampid());

   if (driver->perProcessReplyQueue[pidx]) {
      // Already open!
      return ios::Error::Busy;
   }

   if (!replyQueue || numReplies != IPCKDriverReplyQueue::Size) {
      return ios::Error::Invalid;
   }

   driver->perProcessNumUserRequests[pidx] = 0u;
   driver->perProcessReplyQueue[pidx] = replyQueue;
   driver->perProcessCallbacks[pidx] = callback;
   driver->perProcessCallbackStacks[pidx] = callbackStack;
   driver->perProcessCallbackContexts[pidx] = callbackContext;
   std::memset(replyQueue.getRawPointer(), 0, sizeof(IPCKDriverReplyQueue));
   return ios::Error::OK;
}

ios::Error
IPCKDriver_UserClose(virt_ptr<IPCKDriver> driver)
{
   auto rampid = getRampid();
   auto pidx = static_cast<uint32_t>(getRampid());
   IPCKDriver_CloseAll(driver, rampid, 0);

   // Free any pending requests
   if (driver->perProcessUserReply[pidx].count > 0) {
      virt_ptr<IPCKDriverRequestBlock> block;

      while (IPCKDriver_FIFOPop(virt_addrof(driver->perProcessUserReply[pidx]), &block) == ios::Error::OK) {
         IPCKDriver_FreeRequestBlock(driver, block);
      }
   }

   // Reset the user queue
}

ios::Error
IPCKDriver_LoaderOpen(virt_ptr<IPCKDriver> driver)
{
   auto pidx = static_cast<uint32_t>(getRampid());
   auto kernelProcessId = getKernelProcessId();
   if (kernelProcessId != KernelProcessId::Loader) {
      return ios::Error::Invalid;
   }

   driver->perProcessNumLoaderRequests[pidx] = 0u;
   IPCKDriver_FIFOInit(virt_addrof(driver->perProcessLoaderReply[pidx]));
   return ios::Error::OK;
}

virt_ptr<IPCKDriverRequest>
IPCKDriver_LoaderPollCompletion(virt_ptr<IPCKDriver> driver)
{
   virt_ptr<IPCKDriverRequestBlock> block;
   auto pidx = static_cast<uint32_t>(getRampid());
   auto error = IPCKDriver_FIFOPop(virt_addrof(driver->perProcessLoaderReply[pidx]),
                                   &block);
   if (error < ios::Error::OK) {
      return nullptr;
   }

   // Copy reply to our user request structure
   std::memcpy(block->userRequest.getRawPointer(),
               block->request.getRawPointer(),
               0x48u);

   driver->perProcessNumLoaderRequests[pidx]--;

   auto request = block->userRequest;
   IPCKDriver_FreeRequestBlock(driver, block);
   return request;
}

ios::Error
IPCKDriver_AllocateRequestBlock(RamProcessId clientProcessId,
                                RamProcessId r4,
                                virt_ptr<IPCKDriver> driver,
                                virt_ptr<IPCKDriverRequestBlock> *outRequestBlock,
                                ios::Handle handle,
                                ios::Command command,
                                IPCKDriverAsyncCallbackFn asyncCallback,
                                virt_ptr<void> asyncContext)
{
   auto error = IPCKDriver_FIFOPop(virt_addrof(driver->freeFifo), outRequestBlock);
   if (error < ios::Error::OK) {
      if (error == ios::Error::QEmpty) {
         return ios::Error::QFull;
      }

      return error;
   }

   auto requestBlock = *outRequestBlock;
   requestBlock->flags = (requestBlock->flags & ~0xC000) | 0x4000;
   requestBlock->flags |= (static_cast<uint32_t>(clientProcessId) & 0xFF) << 8;
   requestBlock->flags &= 0xFFFFC3FF;
   *requestBlock->asyncCallback = asyncCallback;
   requestBlock->asyncContext = asyncContext;
   requestBlock->userRequest = nullptr;

   auto request = requestBlock->request;
   std::memset(virt_addrof(request->request.args).getRawPointer(), 0, sizeof(request->request.args));

   request->request.clientPid = static_cast<int32_t>(clientProcessId);
   request->request.handle = handle;
   request->request.command = command;
   request->request.flags = 0u;
   request->request.reply = ios::Error::OK;
   request->request.cpuId = static_cast<ios::CpuId>(cpu::this_core::id() + 1);

   if (clientProcessId != RamProcessId::Kernel) {
      request->request.titleId = getTitleId();
   }

   request->prevHandle = handle;
   request->prevCommand = command;
   return ios::Error::OK;
}

ios::Error
IPCKDriver_AllocateUserRequestBlock(RamProcessId clientProcessId,
                                    RamProcessId r4,
                                    virt_ptr<IPCKDriver> driver,
                                    virt_ptr<IPCKDriverRequestBlock> *outRequestBlock,
                                    virt_ptr<IPCKDriverRequest> userRequest)
{
   auto error = IPCKDriver_AllocateRequestBlock(clientProcessId,
                                                r4,
                                                driver,
                                                outRequestBlock,
                                                0,
                                                ios::Command::Invalid,
                                                IPCKDriverAsyncCallbackFn { },
                                                nullptr);

   if (error >= ios::Error::OK) {
      auto requestBlock = *outRequestBlock;
      requestBlock->userRequest = userRequest;
      requestBlock->flags |= static_cast<uint32_t>(clientProcessId);
   }

   return error;
}

void
IPCKDriver_FreeRequestBlock(virt_ptr<IPCKDriver> driver,
                            virt_ptr<IPCKDriverRequestBlock> requestBlock)
{
   requestBlock->flags &= 0xFFFF3FFFu;
   requestBlock->asyncCallback = nullptr;
   requestBlock->asyncContext = nullptr;
   requestBlock->userRequest = nullptr;
   requestBlock->flags &= 0xFFFFC3FFu;
   IPCKDriver_FIFOPush(virt_addrof(driver->freeFifo), requestBlock);
}

ios::Error
IPCKDriver_SubmitRequest(virt_ptr<IPCKDriver> driver,
                         virt_ptr<IPCKDriverRequestBlock> requestBlock)
{
   auto error = IPCKDriver_FIFOPush(virt_addrof(driver->outboundFIFO),
                                    requestBlock);
   if (error < ios::Error::OK) {
      return error;
   }

   if (driver->state != IPCKDriverState::Open) {
      return ios::Error::NotReady;
   }

   error = IPCKDriver_FIFOPop(virt_addrof(driver->outboundFIFO),
                              &requestBlock);
   if (error < ios::Error::OK) {
      return error;
   }

   ios::kernel::submitIpcRequest(virtualToPhysical(virt_addrof(requestBlock->request->request)));
   return ios::Error::OK;
}

ios::Error
IPCKDriver_ProcessUserRequest(virt_ptr<IPCKDriver> driver,
                              virt_ptr<IPCKDriverRequestBlock> requestBlock)
{
   auto request = requestBlock->request;

   switch (request->request.command) {
   case ios::Command::Open:
      request->prevHandle = 0;
      request->request.args.open.name = phys_cast<char *>(virtualToPhysical(request->buffer1));
      break;
   case ios::Command::Read:
      request->request.args.read.data = virtualToPhysical(request->buffer1);
      break;
   case ios::Command::Write:
      request->request.args.write.data = virtualToPhysical(request->buffer1);
      break;
   case ios::Command::Ioctl:
      request->request.args.ioctl.inputBuffer = virtualToPhysical(request->buffer1);
      request->request.args.ioctl.outputBuffer = virtualToPhysical(request->buffer2);
      break;
   case ios::Command::Ioctlv:
      request->request.args.ioctlv.vecs = phys_cast<ios::IoctlVec *>(virtualToPhysical(request->buffer1));
      break;
   default:
      break;
   }

   return ios::Error::OK;
}

ios::Error
IPCKDriver_SubmitLoaderOrUserRequest(virt_ptr<IPCKDriver> driver,
                                     virt_ptr<IPCKDriverRequest> userRequest,
                                     bool isLoader)
{
   auto error = ios::Error::OK;
   auto rampid = getRampid();
   auto pidx = static_cast<uint32_t>(rampid);

   if (!driver) {
      error = ios::Error::Invalid;
   } else {
      if (driver->perProcessNumUserRequests[pidx] +
          driver->perProcessNumLoaderRequests[pidx] >= IPCKRequestsPerProcess) {
         error = ios::Error::QFull;
      } else {
         virt_ptr<IPCKDriverRequestBlock> requestBlock;
         error = IPCKDriver_AllocateUserRequestBlock(isLoader ? RamProcessId::Kernel : rampid,
                                                     isLoader ? rampid : RamProcessId::Invalid,
                                                     driver,
                                                     &requestBlock,
                                                     userRequest);
         if (error >= ios::Error::OK) {
            std::memcpy(requestBlock->request.getRawPointer(),
                        userRequest.getRawPointer(),
                        0x48u);

            error = IPCKDriver_ProcessUserRequest(driver, requestBlock);
            if (error >= ios::Error::OK) {
               error = IPCKDriver_SubmitRequest(driver, requestBlock);
            }

            if (isLoader) {
               driver->perProcessNumLoaderRequests[pidx]++;
            } else {
               driver->perProcessNumUserRequests[pidx]++;
            }

            if (error < ios::Error::OK) {
               IPCKDriver_FreeRequestBlock(driver, requestBlock);
            }
         }
      }
   }

   driver->perProcessLastError[pidx] = error;
   return ios::Error::OK;
}


static ios::Error
defensiveProcessIncomingMessagePointer(virt_ptr<IPCKDriver> driver,
                                       virt_ptr<IPCKDriverRequest> request,
                                       virt_ptr<IPCKDriverRequestBlock> *outRequestBlock)
{
   auto index = request - driver->requestsBuffer;
   if (index >= IPCKRequestsPerCore || index < 0) {
      return ios::Error::Invalid;
   }

   if (driver->requestBlocks[index].request != request) {
      return ios::Error::Invalid;
   }

   auto requestBlock = virt_addrof(driver->requestBlocks[index]);
   if ((requestBlock->flags & 0xC000) == 0) {
      return ios::Error::Invalid;
   }

   requestBlock->flags &= ~0x3000;
   requestBlock->flags |= 0x1000;
   *outRequestBlock = requestBlock;
   return ios::Error::OK;
}

static void
IPCKDriver_DispatchUserReplies(uint32_t pidx,
                               virt_ptr<void> /*argv*/)
{
   virt_ptr<IPCKDriverRequestBlock> requestBlock;
   auto driver = IPCKDriver_GetInstance();
   auto replyQueue = driver->perProcessReplyQueue[pidx];

   while (replyQueue->numReplies < replyQueue->replies.size()) {
      auto error = IPCKDriver_FIFOPop(virt_addrof(driver->perProcessUserReply[pidx]),
                                      &requestBlock);
      if (error < ios::Error::OK) {
         break;
      }

      // TODO: We probably don't want to memcpy the whole structure
      std::memcpy(requestBlock->userRequest.getRawPointer(),
                  requestBlock->request.getRawPointer(),
                  0x48u);

      replyQueue->replies[replyQueue->numReplies] = requestBlock->userRequest;
      replyQueue->numReplies++;

      driver->perProcessNumUserRequests[pidx]--;
      IPCKDriver_FreeRequestBlock(driver, requestBlock);
   }

   // Queue a user callback
   queueUserExceptionCallback(driver->perProcessCallbacks[pidx],
                              driver->perProcessCallbackContexts[pidx],
                              driver->perProcessCallbackStacks[pidx]);
}

void
KiSetExExit()
{
}

void
IPCKDriver_ProcessReply(virt_ptr<IPCKDriver> driver,
                        virt_ptr<ios::IpcRequest> reply)
{
   if (driver->state < IPCKDriverState::Open) {
      return;
   }

   virt_ptr<IPCKDriverRequestBlock> requestBlock;
   auto request = virt_cast<IPCKDriverRequest *>(reply);
   auto error = defensiveProcessIncomingMessagePointer(driver, request, &requestBlock);
   if (error < ios::Error::OK) {
      return;
   }

   auto pidx = requestBlock->flags & 0xFF;

   if (requestBlock->asyncCallback) {
      (*requestBlock->asyncCallback)(reply->reply, requestBlock->asyncContext);
   } else {
      auto rampid = getRampid();
      auto pidx = static_cast<uint32_t>(rampid);

      auto r30 = 0u;
      auto r12 = (requestBlock->flags >> 8) & 0xFF;
      auto r24 = (requestBlock->flags >> 0) & 0xFF;

      if (r12 != -1 && r12 != 0) {
         r24 = r12;
         r30 = 1;
      }

      r12 = r24 * 4;
      if (!driver->perProcessReplyQueue[r24] && r30 == 0) {
         //  O SHIT Something bad happend!
      } else {

         if (r24 != (unsigned)rampid) {

         }
      }

      if (false) { // TODO: How to read flags to check if loader or user request
         IPCKDriver_FIFOPush(virt_addrof(driver->perProcessLoaderReply[pidx]),
                             requestBlock);
      } else {
         IPCKDriver_FIFOPush(virt_addrof(driver->perProcessUserReply[pidx]),
                             requestBlock);

         // Queue a kernel callback to handle the pending replies
         queueKernelExceptionCallback(&IPCKDriver_DispatchUserReplies, pidx, nullptr);
      }
   }

   IPCKDriver_FreeRequestBlock(driver, requestBlock);
}

ios::Error
initialiseIpckDriver()
{
   auto driver = IPCKDriver_GetInstance();
   std::memset(driver.getRawPointer(), 0, sizeof(IPCKDriver));

   driver->coreId = cpu::this_core::id();

   switch (driver->coreId) {
   case 0:
      driver->ipcCommand = ios::Command::IpcMsg0;
      driver->requestsBuffer = virt_addrof(sIpckData->requestBuffer);
      break;
   case 1:
      driver->ipcCommand = ios::Command::IpcMsg1;
      driver->requestsBuffer = virt_addrof(sIpckData->requestBuffer) + IPCKRequestsPerCore;
      break;
   case 2:
      driver->ipcCommand = ios::Command::IpcMsg2;
      driver->requestsBuffer = virt_addrof(sIpckData->requestBuffer) + IPCKRequestsPerCore * 2;
      break;
   }

   auto error = IPCKDriver_Init(driver);
   if (error < ios::Error::OK) {
      return error;
   }

   std::memset(driver->requestsBuffer.getRawPointer(), 0, sizeof(IPCKDriverRequest) * IPCKRequestsPerCore);
   driver->state = IPCKDriverState::Initialised;

   // one time initialisation for something.
   static bool initialisedSomething = false;
   if (!initialisedSomething) {
      // lock init
      // lock acquire

      // PER PROCESS LOOP
      for (auto i = 0u; i < 8u; ++i) {
         // unk(0x10000000, i * 0xC + 0xFFE85B00, 0x60)
         for (auto j = 0u; j < 0x60u; ++j) {
            // *(uint32_t*)(i * 0x180 + 0xFFE85B60 + j * 4) = -1;
         }
      }

      // lock release

      // TODO: add proc action callback
      initialisedSomething = true;
   }

   return ios::Error::OK;
}

ios::Error
openIpckDriver()
{
   auto driver = IPCKDriver_GetInstance();
   if (driver->state != IPCKDriverState::Initialised && driver->state != IPCKDriverState::Unknown1) {
      return ios::Error::NotReady;
   }

   auto error = IPCKDriver_Open(driver);
   if (error < ios::Error::OK) {
      return error;
   }

   IPCKDriver_FIFOInit(virt_addrof(driver->freeFifo));
   IPCKDriver_FIFOInit(virt_addrof(driver->outboundFIFO));

   for (auto i = 0u; i < NumProcesses; ++i) {
      IPCKDriver_FIFOInit(virt_addrof(driver->perProcessUserReply[i]));
      IPCKDriver_FIFOInit(virt_addrof(driver->perProcessLoaderReply[i]));
   }

   for (auto i = 0u; i < IPCKRequestsPerCore; ++i) {
      IPCKDriver_FIFOPush(virt_addrof(driver->freeFifo),
                          virt_addrof(driver->requestBlocks[i]));
   }

   // set 0xd0, 0xd4 to something

   driver->unk0x04++;

   // Set PPC CTRL registers
   // Register shenanigans...

   driver->state = IPCKDriverState::Open;
   return ios::Error::OK;
}

} // namespace internal

} // namespace cafe::kernel

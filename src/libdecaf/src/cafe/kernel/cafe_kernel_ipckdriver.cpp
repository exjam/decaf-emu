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

constexpr auto IPCKRequestsPerCore = 0xB0u;
constexpr auto IPCKRequestsPerProcess = 0x30u;

enum class IPCKDriverState : uint32_t
{
   Invalid = 0,
   Unknown1 = 1,
   Initialised = 2,
   Open = 3,
   Submitting = 4,
};

#pragma pack(push, 1)
struct IPCKDriverRegisters
{
   be2_virt_ptr<void> ppcMsg;
   be2_virt_ptr<void> ppcCtrl; // 0xc0
   be2_virt_ptr<void> armMsg;
   be2_virt_ptr<void> ahbLt;
   be2_val<uint32_t> unkMaybeFlags;
};

struct IPCKDriverRequestBlock
{
   be2_val<uint32_t> flags;
   be2_virt_ptr<void> asyncCallback; // func_ptr<void(ios::Error,virt_ptr<void>)>
   be2_virt_ptr<void> asyncContext;
   be2_virt_ptr<IPCKDriverRequest> userRequest;
   be2_virt_ptr<IPCKDriverRequest> request;
};

/**
 * FIFO queue for IPCKDriverRequestBlocks.
 *
 * Functions similar to a ring buffer.
 */
template<std::size_t Size>
struct IPCKDriverFIFO
{
   //! The current item index to push to
   be2_val<int32_t> pushIndex;

   //! The current item index to pop from
   be2_val<int32_t> popIndex;

   //! The number of items in the queue
   be2_val<int32_t> count;

   //! Tracks the highest amount of items there has been in the queue
   be2_val<int32_t> maxCount;

   //! Items in the queue
   be2_array<virt_ptr<IPCKDriverRequestBlock>, Size> requestBlocks;
};
CHECK_OFFSET(IPCKDriverFIFO<1>, 0x00, pushIndex);
CHECK_OFFSET(IPCKDriverFIFO<1>, 0x04, popIndex);
CHECK_OFFSET(IPCKDriverFIFO<1>, 0x08, count);
CHECK_OFFSET(IPCKDriverFIFO<1>, 0x0C, maxCount);
CHECK_OFFSET(IPCKDriverFIFO<1>, 0x10, requestBlocks);
CHECK_SIZE(IPCKDriverFIFO<1>, 0x14);

struct IPCKDriver
{
   be2_val<IPCKDriverState> state;
   be2_val<uint32_t> unk0x04;
   be2_val<uint32_t> coreId;
   be2_val<ios::Command> ipcCommand;
   be2_virt_ptr<IPCKDriverRequest> requestsBuffer;
   UNKNOWN(4);
   be2_array<virt_ptr<IPCKDriverReplyQueue>, NumProcesses> perProcessReplyQueue;
   be2_array<virt_ptr<void>, NumProcesses> perProcessCallbacks;
   be2_array<virt_ptr<void>, NumProcesses> perProcessCallbackStacks;
   be2_array<virt_ptr<void>, NumProcesses> perProcessCallbackContexts;
   be2_array<ios::Error, NumProcesses> perProcessLastError;
   UNKNOWN(0x4);
   be2_struct<IPCKDriverRegisters> registers;
   UNKNOWN(0x8);
   be2_array<uint32_t, NumProcesses> perProcessNumOutboundRequests;
   be2_array<uint32_t, NumProcesses> perProcessUnk0xF8;
   be2_struct<IPCKDriverFIFO<IPCKRequestsPerCore>> freeFifo;
   be2_struct<IPCKDriverFIFO<IPCKRequestsPerCore>> outboundFIFO;
   be2_array<IPCKDriverFIFO<IPCKRequestsPerProcess>, NumProcesses> perProcessUserReply;
   be2_array<IPCKDriverFIFO<IPCKRequestsPerProcess>, NumProcesses> perProcessLoaderReply;
   be2_array<IPCKDriverRequestBlock, IPCKRequestsPerCore> requestBlocks;
};
CHECK_OFFSET(IPCKDriver, 0x0, state);
CHECK_OFFSET(IPCKDriver, 0x4, unk0x04);
CHECK_OFFSET(IPCKDriver, 0x8, coreId);
CHECK_OFFSET(IPCKDriver, 0xC, ipcCommand);
CHECK_OFFSET(IPCKDriver, 0x10, requestsBuffer);
CHECK_OFFSET(IPCKDriver, 0x18, perProcessReplyQueue);
CHECK_OFFSET(IPCKDriver, 0x38, perProcessCallbacks);
CHECK_OFFSET(IPCKDriver, 0x58, perProcessCallbackStacks);
CHECK_OFFSET(IPCKDriver, 0x78, perProcessCallbackContexts);
CHECK_OFFSET(IPCKDriver, 0x98, perProcessLastError);
CHECK_OFFSET(IPCKDriver, 0xBC, registers);
CHECK_OFFSET(IPCKDriver, 0xD8, perProcessNumOutboundRequests);
CHECK_OFFSET(IPCKDriver, 0xF8, perProcessUnk0xF8);
CHECK_OFFSET(IPCKDriver, 0x118, freeFifo);
CHECK_OFFSET(IPCKDriver, 0x3E8, outboundFIFO);
CHECK_OFFSET(IPCKDriver, 0x6B8, perProcessUserReply);
CHECK_OFFSET(IPCKDriver, 0xD38, perProcessLoaderReply);
CHECK_OFFSET(IPCKDriver, 0x13B8, requestBlocks);
CHECK_SIZE(IPCKDriver, 0x2178);

#pragma pack(pop)

void
IPCKDriver_FreeRequestBlock(virt_ptr<IPCKDriver> driver,
                            virt_ptr<IPCKDriverRequestBlock> requestBlock);

struct StaticIpckData
{
   be2_array<IPCKDriver, 3> drivers;
   be2_array<IPCKDriverRequest, IPCKRequestsPerCore * 3> requestBuffer;
};

static virt_ptr<StaticIpckData>
sIpckData;

template<std::size_t Size>
void
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
ios::Error
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
ios::Error
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

/**
 * Peek the next request which would be popped from a IPCKDriverFIFO structure.
 *
 * \retval ios::Error::OK
 * Success.
 *
 * \retval ios::Error::QEmpty
 * There was no requests to pop from the queue.
 */
template<std::size_t Size>
ios::Error
IPCKDriver_PeekFIFO(virt_ptr<IPCKDriverFIFO<Size>> fifo,
                    virt_ptr<IPCKDriverRequestBlock > *outRequestBlock)
{
   if (fifo->popIndex == -1) {
      return ios::Error::QEmpty;
   }

   *outRequestBlock = fifo->requestBlocks[fifo->popIndex];
   return ios::Error::OK;
}

virt_ptr<IPCKDriver>
IPCKDriver_GetInstance()
{
   // core id * 0x2178 + 0xFFE86760 (static memory)
   return virt_addrof(sIpckData->drivers[cpu::this_core::id()]);
}

ios::Error
IPCKDriver_Open(virt_ptr<IPCKDriver> driver)
{
   for (auto i = 0u; i < IPCKRequestsPerCore; ++i) {
      driver->requestBlocks[i].asyncCallback = nullptr;
      driver->requestBlocks[i].asyncContext = nullptr;
      driver->requestBlocks[i].userRequest = nullptr;
      driver->requestBlocks[i].request = virt_addrof(driver->requestsBuffer[i]);
   }

   return ios::Error::OK;
}

ios::Error
IPCKDriver_LoaderOpen()
{
   auto driver = IPCKDriver_GetInstance();
   auto pidx = static_cast<uint32_t>(getRampid());
   auto kernelProcessId = getKernelProcessId();

   if (kernelProcessId != KernelProcessId::Loader) {
      return ios::Error::Invalid;
   }

   driver->perProcessUnk0xF8[pidx] = 0u;
   IPCKDriver_FIFOInit(virt_addrof(driver->perProcessLoaderReply[pidx]));
   return ios::Error::OK;
}

ios::Error
IPCKDriver_UserOpen(uint32_t numReplies,
                    virt_ptr<IPCKDriverReplyQueue> replyQueue,
                    virt_ptr<void> callback, // func_ptr
                    virt_ptr<void> callbackStack,
                    virt_ptr<void> callbackContext)
{
   auto driver = IPCKDriver_GetInstance();
   auto rampid = getRampid();
   auto pidx = static_cast<uint32_t>(getRampid());

   if (driver->perProcessReplyQueue[pidx]) {
      // Already open!
      return ios::Error::Busy;
   }

   if (!replyQueue || numReplies != IPCKDriverReplyQueue::Size) {
      return ios::Error::Invalid;
   }

   driver->perProcessNumOutboundRequests[pidx] = 0u;
   driver->perProcessReplyQueue[pidx] = replyQueue;
   driver->perProcessCallbacks[pidx] = callback;
   driver->perProcessCallbackStacks[pidx] = callbackStack;
   driver->perProcessCallbackContexts[pidx] = callbackContext;
   std::memset(replyQueue.getRawPointer(), 0, sizeof(IPCKDriverReplyQueue));
   return ios::Error::OK;
}

ios::Error
IPCKDriver_Init(virt_ptr<IPCKDriver> driver)
{
   switch (driver->coreId) {
   case 0:
      driver->registers.ppcMsg  = virt_cast<void *>(virt_addr { 0x0D800400 });
      driver->registers.ppcCtrl = virt_cast<void *>(virt_addr { 0x0D800404 });
      driver->registers.armMsg  = virt_cast<void *>(virt_addr { 0x0D800408 });
      driver->registers.ahbLt   = virt_cast<void *>(virt_addr { 0x0D800444 });
      driver->registers.unkMaybeFlags = 0x40000000u;
      break;
   case 1:
      driver->registers.ppcMsg  = virt_cast<void *>(virt_addr { 0x0D800410 });
      driver->registers.ppcCtrl = virt_cast<void *>(virt_addr { 0x0D800414 });
      driver->registers.armMsg  = virt_cast<void *>(virt_addr { 0x0D800418 });
      driver->registers.ahbLt   = virt_cast<void *>(virt_addr { 0x0D800454 });
      driver->registers.unkMaybeFlags = 0x10000000u;
      break;
   case 2:
      driver->registers.ppcMsg  = virt_cast<void *>(virt_addr { 0x0D800420 });
      driver->registers.ppcCtrl = virt_cast<void *>(virt_addr { 0x0D800424 });
      driver->registers.armMsg  = virt_cast<void *>(virt_addr { 0x0D800428 });
      driver->registers.ahbLt   = virt_cast<void *>(virt_addr { 0x0D800464 });
      driver->registers.unkMaybeFlags = 0x04000000u;
      break;
   }

   return ios::Error::OK;
}

ios::Error
initIpckDriver()
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

static ios::Error
defensiveProcessIncomingMessagePointer(virt_ptr<IPCKDriver> driver,
                                       virt_ptr<cafe::kernel::IPCKDriverRequest> request,
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

ios::Error
IPCKDriver_ProcessRequest(virt_ptr<IPCKDriver> driver,
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

   if (!requestBlock->asyncCallback) {
      auto pidx = static_cast<uint32_t>(getRampid());

      // TODO: Logic to put this into perProcessLoaderReply
      IPCKDriver_FIFOPush(virt_addrof(driver->perProcessUserReply),
                          requestBlock);

      // TODO: How does this get around to the async callback being called???
   } else {
      // Call requestBlock->asyncCallback immediately
   }

   IPCKDriver_FreeRequestBlock(driver, requestBlock);
}
template <typename> struct Debug;
ios::Error
IPCKDriver_AllocateRequestBlock(RamProcessId r3,
                                RamProcessId r4,
                                virt_ptr<IPCKDriver> driver,
                                virt_ptr<IPCKDriverRequestBlock> *outRequestBlock,
                                int32_t handle,
                                ios::Command command,
                                virt_ptr<void> asyncCallback,
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
   requestBlock->flags &= ~0xC000;
   requestBlock->flags |= 0x4000;
   requestBlock->flags |= (static_cast<uint32_t>(r3) & 0xFF) << 8;
   requestBlock->flags &= 0xFFFFC3FF;
   requestBlock->asyncCallback = asyncCallback;
   requestBlock->asyncContext = asyncContext;
   requestBlock->userRequest = nullptr;

   auto request = requestBlock->request;
   std::memset(virt_addrof(request->request.args).getRawPointer(), 0, sizeof(request->request.args));

   request->request.clientPid = static_cast<int32_t>(r3);
   request->request.handle = handle;
   request->request.command = command;
   request->request.flags = 0u;
   request->request.reply = ios::Error::OK;
   request->request.cpuId = static_cast<ios::CpuId>(cpu::this_core::id() + 1);

   if (r3 != RamProcessId::Kernel) {
      // TODO: Get Title ID
      request->request.titleId = 0ull;
   }

   request->prevHandle = handle;
   request->prevCommand = command;
   return ios::Error::OK;
}

ios::Error
IPCKDriver_AllocateUserRequestBlock(RamProcessId r3,
                                    RamProcessId r4,
                                    virt_ptr<IPCKDriver> driver,
                                    virt_ptr<IPCKDriverRequestBlock> *outRequestBlock,
                                    virt_ptr<IPCKDriverRequest> userRequest)
{
   auto error = IPCKDriver_AllocateRequestBlock(r3,
                                                r4,
                                                driver,
                                                outRequestBlock,
                                                0,
                                                ios::Command::Invalid,
                                                nullptr,
                                                nullptr);

   if (error >= ios::Error::OK) {
      auto requestBlock = *outRequestBlock;
      requestBlock->userRequest = userRequest;
      requestBlock->flags |= static_cast<uint32_t>(r3);
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
IPCKDriver_SubmitUserRequest(virt_ptr<IPCKDriverRequest> userRequest)
{
   auto error = ios::Error::OK;
   auto rampid = getRampid();
   auto pidx = static_cast<uint32_t>(rampid);

   auto driver = IPCKDriver_GetInstance();
   if (!driver) {
      error = ios::Error::Invalid;
   } else {
      if (driver->perProcessNumOutboundRequests[pidx] +
          driver->perProcessUnk0xF8[pidx] >= IPCKRequestsPerProcess) {
         error = ios::Error::QFull;
      } else {
         virt_ptr<IPCKDriverRequestBlock> requestBlock;
         error = IPCKDriver_AllocateUserRequestBlock(rampid,
                                                     rampid,
                                                     driver,
                                                     &requestBlock,
                                                     userRequest);
         if (error >= ios::Error::OK) {
            std::memcpy(requestBlock->request.getRawPointer(),
                        userRequest.getRawPointer(),
                        0x48u);

            error = IPCKDriver_ProcessRequest(driver, requestBlock);
            if (error >= ios::Error::OK) {
               error = IPCKDriver_SubmitRequest(driver, requestBlock);
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
   IPCKDriver_ProcessReply(IPCKDriver_GetInstance(), physicalToVirtual(reply));
}

} // namespace cafe::kernel

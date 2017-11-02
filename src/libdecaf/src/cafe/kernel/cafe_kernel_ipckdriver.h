#pragma once
#include "cafe_kernel_context.h"
#include "cafe_kernel_exception.h"
#include "cafe_kernel_process.h"
#include "ios/ios_ipc.h"

#include <cstdint>
#include <functional>
#include <libcpu/be2_struct.h>

namespace cafe::kernel
{

#pragma pack(push, 1)

struct IPCKDriverRequest
{
   //! Actual IPC request
   be2_struct<ios::IpcRequest> request;

   //! Allegedly the previous IPC command
   be2_val<ios::Command> prevCommand;

   //! Allegedly the previous IPC handle
   be2_val<int32_t> prevHandle;

   //! Buffer argument 1
   be2_virt_ptr<void> buffer1;

   //! Buffer argument 2
   be2_virt_ptr<void> buffer2;

   //! Buffer to copy device name to for IOS_Open
   be2_array<char, 0x20> nameBuffer;

   UNKNOWN(0x80 - 0x68);
};
CHECK_OFFSET(IPCKDriverRequest, 0x00, request);
CHECK_OFFSET(IPCKDriverRequest, 0x38, prevCommand);
CHECK_OFFSET(IPCKDriverRequest, 0x3C, prevHandle);
CHECK_OFFSET(IPCKDriverRequest, 0x40, buffer1);
CHECK_OFFSET(IPCKDriverRequest, 0x44, buffer2);
CHECK_OFFSET(IPCKDriverRequest, 0x48, nameBuffer);
CHECK_SIZE(IPCKDriverRequest, 0x80);

struct IPCKDriverReplyQueue
{
   static constexpr auto Size = 0x30u;
   be2_val<uint32_t> numReplies;
   be2_array<virt_ptr<IPCKDriverRequest>, Size> replies;
};
CHECK_OFFSET(IPCKDriverReplyQueue, 0x00, numReplies);
CHECK_OFFSET(IPCKDriverReplyQueue, 0x04, replies);
CHECK_SIZE(IPCKDriverReplyQueue, 0xC4);

void
handleIpcInterrupt();

void
submitIpcReply(phys_ptr<ios::IpcRequest> reply);

ios::Error
userOpenIpckDriver(uint32_t numReplies,
                   virt_ptr<IPCKDriverReplyQueue> replyQueue,
                   UserExceptionCallbackFn callback,
                   virt_ptr<void> callbackStack,
                   virt_ptr<Context> callbackContext);

ios::Error
userCloseIpckDriver();

ios::Error
userSubmitIpckRequest(virt_ptr<IPCKDriverRequest> request);

ios::Error
loaderOpenIpckDriver();

ios::Error
loaderSubmitIpckRequest(virt_ptr<IPCKDriverRequest> request);

virt_ptr<IPCKDriverRequest>
loaderPollIpckCompletion();

namespace internal
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

struct IPCKDriverRegisters
{
   be2_virt_ptr<void> ppcMsg;
   be2_virt_ptr<void> ppcCtrl; // 0xc0
   be2_virt_ptr<void> armMsg;
   be2_virt_ptr<void> ahbLt;
   be2_val<uint32_t> unkMaybeFlags;
};

#ifdef DECAF_KERNEL_LLE
using IPCKDriverAsyncCallbackFn = virt_func_ptr<void(ios::Error, virt_ptr<void>)>;

struct IPCKDriverRequestBlock
{
   be2_val<uint32_t> flags;
   be2_val<IPCKDriverAsyncCallbackFn> asyncCallback;
   be2_virt_ptr<void> asyncContext;
   be2_virt_ptr<IPCKDriverRequest> userRequest;
   be2_virt_ptr<IPCKDriverRequest> request;
};
CHECK_OFFSET(IPCKDriverRequestBlock, 0x00, flags);
CHECK_OFFSET(IPCKDriverRequestBlock, 0x04, asyncCallback);
CHECK_OFFSET(IPCKDriverRequestBlock, 0x08, asyncContext);
CHECK_OFFSET(IPCKDriverRequestBlock, 0x0C, userRequest);
CHECK_OFFSET(IPCKDriverRequestBlock, 0x10, request);
CHECK_SIZE(IPCKDriverRequestBlock, 0x14);
#else
using IPCKDriverAsyncCallbackFn = std::function<void(ios::Error, virt_ptr<void>)>;

struct IPCKDriverRequestBlock
{
   be2_val<uint32_t> flags;
   be2_virt_ptr<IPCKDriverAsyncCallbackFn> asyncCallback;
   be2_virt_ptr<void> asyncContext;
   be2_virt_ptr<IPCKDriverRequest> userRequest;
   be2_virt_ptr<IPCKDriverRequest> request;
};
CHECK_OFFSET(IPCKDriverRequestBlock, 0x00, flags);
CHECK_OFFSET(IPCKDriverRequestBlock, 0x04, asyncCallback);
CHECK_OFFSET(IPCKDriverRequestBlock, 0x08, asyncContext);
CHECK_OFFSET(IPCKDriverRequestBlock, 0x0C, userRequest);
CHECK_OFFSET(IPCKDriverRequestBlock, 0x10, request);
CHECK_SIZE(IPCKDriverRequestBlock, 0x14);
#endif

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
   be2_array<UserExceptionCallbackFn, NumProcesses> perProcessCallbacks;
   be2_array<virt_ptr<void>, NumProcesses> perProcessCallbackStacks;
   be2_array<virt_ptr<Context>, NumProcesses> perProcessCallbackContexts;
   be2_array<ios::Error, NumProcesses> perProcessLastError;
   UNKNOWN(0x4);
   be2_struct<IPCKDriverRegisters> registers;
   UNKNOWN(0x8);
   be2_array<uint32_t, NumProcesses> perProcessNumUserRequests;
   be2_array<uint32_t, NumProcesses> perProcessNumLoaderRequests;
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
CHECK_OFFSET(IPCKDriver, 0xD8, perProcessNumUserRequests);
CHECK_OFFSET(IPCKDriver, 0xF8, perProcessNumLoaderRequests);
CHECK_OFFSET(IPCKDriver, 0x118, freeFifo);
CHECK_OFFSET(IPCKDriver, 0x3E8, outboundFIFO);
CHECK_OFFSET(IPCKDriver, 0x6B8, perProcessUserReply);
CHECK_OFFSET(IPCKDriver, 0xD38, perProcessLoaderReply);
CHECK_OFFSET(IPCKDriver, 0x13B8, requestBlocks);
CHECK_SIZE(IPCKDriver, 0x2178);

#pragma pack(pop)

virt_ptr<IPCKDriver>
IPCKDriver_GetInstance();

ios::Error
IPCKDriver_Init(virt_ptr<IPCKDriver> driver);

ios::Error
IPCKDriver_Open(virt_ptr<IPCKDriver> driver);

ios::Error
IPCKDriver_UserOpen(virt_ptr<IPCKDriver> driver,
                    uint32_t numReplies,
                    virt_ptr<IPCKDriverReplyQueue> replyQueue,
                    UserExceptionCallbackFn callback,
                    virt_ptr<void> callbackStack,
                    virt_ptr<Context> callbackContext);

ios::Error
IPCKDriver_UserClose(virt_ptr<IPCKDriver> driver);

ios::Error
IPCKDriver_LoaderOpen(virt_ptr<IPCKDriver> driver);

virt_ptr<IPCKDriverRequest>
IPCKDriver_LoaderPollCompletion(virt_ptr<IPCKDriver> driver);

ios::Error
IPCKDriver_AllocateRequestBlock(RamProcessId clientProcessId,
                                RamProcessId r4,
                                virt_ptr<IPCKDriver> driver,
                                virt_ptr<IPCKDriverRequestBlock> *outRequestBlock,
                                ios::Handle handle,
                                ios::Command command,
                                IPCKDriverAsyncCallbackFn asyncCallback,
                                virt_ptr<void> asyncContext);

ios::Error
IPCKDriver_AllocateUserRequestBlock(RamProcessId clientProcessId,
                                    RamProcessId r4,
                                    virt_ptr<IPCKDriver> driver,
                                    virt_ptr<IPCKDriverRequestBlock> *outRequestBlock,
                                    virt_ptr<IPCKDriverRequest> userRequest);

void
IPCKDriver_FreeRequestBlock(virt_ptr<IPCKDriver> driver,
                            virt_ptr<IPCKDriverRequestBlock> requestBlock);

ios::Error
IPCKDriver_SubmitRequest(virt_ptr<IPCKDriver> driver,
                         virt_ptr<IPCKDriverRequestBlock> requestBlock);

ios::Error
IPCKDriver_ProcessUserRequest(virt_ptr<IPCKDriver> driver,
                              virt_ptr<IPCKDriverRequestBlock> requestBlock);

ios::Error
IPCKDriver_SubmitLoaderOrUserRequest(virt_ptr<IPCKDriver> driver,
                                     virt_ptr<IPCKDriverRequest> userRequest,
                                     bool isLoader);

void
IPCKDriver_FreeRequestBlock(virt_ptr<IPCKDriver> driver,
                            virt_ptr<IPCKDriverRequestBlock> requestBlock);

void
IPCKDriver_ProcessReply(virt_ptr<IPCKDriver> driver,
                        virt_ptr<ios::IpcRequest> reply);

ios::Error
initialiseIpckDriver();

ios::Error
openIpckDriver();

} // namespace internal

} // namespace cafe::kernel

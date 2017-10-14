#pragma once
#include "ios/ios_ipc.h"

#include <cstdint>
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

#pragma pack(pop)

ios::Error
initIpckDriver();

void
handleIpcInterrupt();

void
IPCKDriver_Init();

// This is the syscall version, different args...
void
IPCKDriver_Open(uint32_t mode,
                void *unkbuffer,
                void *callback,
                void *callbackStack,
                void *callbackContext);

void
IPCKDriver_Close();

uint32_t
IPCKDriver_OpenMCP();

ios::Error
IPCKDriver_SubmitUserRequest(virt_ptr<IPCKDriverRequest> userRequest);

virt_ptr<ios::IpcRequest>
IPCKDriver_PollLoaderCompletion();

} // namespace cafe::kernel

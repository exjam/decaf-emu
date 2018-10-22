#pragma once
#include "ios_nn_recursivemutex.h"
#include "ios_nn_thread.h"

#include "ios/ios_enum.h"
#include "ios/ios_ipc.h"
#include "ios/kernel/ios_kernel_messagequeue.h"
#include "ios/kernel/ios_kernel_resourcemanager.h"
#include "ios/kernel/ios_kernel_thread.h"
#include "ios/kernel/ios_kernel_timer.h"
#include "nn/ipc/nn_ipc_command.h"
#include "nn/ipc/nn_ipc_service.h"
#include "nn/nn_result.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ios::nn
{

using CommandId = ::nn::ipc::CommandId;
using ServiceId = ::nn::ipc::ServiceId;
using Result = ::nn::Result;

struct IoctlvVecs
{
   uint32_t numVecsIn;
   uint32_t numVecsOut;
   phys_ptr<IoctlVec> vecs;
};

struct CommandHandlerArgs
{
   phys_ptr<void> requestBuffer;
   uint32_t requestBufferSize;
   phys_ptr<void> responseBuffer;
   uint32_t responseBufferSize;
   IoctlvVecs vecs;
};

using DeviceHandleId = uint32_t;

using CommandHandler = Result(*)(uint32_t unk1,
                                 CommandId command,
                                 CommandHandlerArgs &args);

class MetaServer
{
   static constexpr auto MaxNumDeviceHandles = 32u;
   static constexpr auto MaxNumServices = 32u;

public:
   struct DeviceHandle
   {
      bool open = false;
      uint64_t caps;
      ProcessId processId;
      RecursiveMutex mutex;
   };

   struct RegisteredService
   {
      ServiceId id;
      CommandHandler handler = nullptr;
   };

   MetaServer(bool isMcpResourceManager = false,
              kernel::ResourcePermissionGroup group = kernel::ResourcePermissionGroup::None) :
      mIsMcpResourceManager(isMcpResourceManager),
      mResourcePermissionGroup(group)
   {
   }

   Result initialise(std::string_view deviceName,
                     phys_ptr<kernel::Message> messageBuffer,
                     uint32_t numMessages);

   void registerService(ServiceId serviceId, CommandHandler commandHandler);

   template<typename ServiceType>
   void registerService()
   {
      registerService(ServiceType::id, ServiceType::commandHandler);
   }

   Result start(phys_ptr<uint8_t> stackTop, uint32_t stackSize,
                kernel::ThreadPriority priority);

protected:
   static Error threadEntryWrapper(phys_ptr<void> ptr);

   Error threadEntry();
   Error waitForResume();
   Error runMessageLoop();
   Error resolvePendingMessages();

   virtual void intialiseServer() { }
   virtual void finaliseServer() { }

   Error openDeviceHandle(uint64_t caps, ProcessId processId);
   Error closeDeviceHandle(DeviceHandleId handleId);
   Error handleMessage(phys_ptr<kernel::ResourceRequest> request);

protected:
   bool mIsMcpResourceManager;
   kernel::MessageQueueId mQueueId = -1;
   kernel::TimerId mTimer = -1;
   kernel::Message mTimerMessage = static_cast<::ios::kernel::Message>(-32);
   Thread mThread;
   std::string mDeviceName;
   std::array<DeviceHandle, MaxNumDeviceHandles> mDeviceHandles;
   kernel::ResourcePermissionGroup mResourcePermissionGroup;

   RecursiveMutex mMutex;
   std::array<RegisteredService, MaxNumServices> mServices;
   IpcRequestArgsResume mResumeArgs;
};

} // namespace ios::nn

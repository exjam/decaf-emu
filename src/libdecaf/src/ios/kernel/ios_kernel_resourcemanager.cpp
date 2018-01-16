#include "ios_kernel_ipc_thread.h"
#include "ios_kernel_messagequeue.h"
#include "ios_kernel_resourcemanager.h"
#include "ios_kernel_process.h"
#include "ios/ios_enum_string.h"
#include "ios/ios_stackobject.h"
#include "kernel/kernel_ipc.h"

#include <common/log.h>
#include <map>
#include <string>
#include <string_view>

namespace ios::kernel
{

struct ResourceData
{
   ResourceManagerList resourceManagerList;
   ResourceRequestList resourceRequestList;
   be2_array<ResourceHandleManager, ProcessID::Max> resourceHandleManagers;
   be2_val<uint32_t> totalOpenedHandles;
};

static phys_ptr<ResourceData>
sData;

namespace internal
{

static Error
findResourceManager(std::string_view device,
                    phys_ptr<ResourceManager> *outResourceManager);

static phys_ptr<ResourceHandleManager>
getResourceHandleManager(ProcessID id);

static Error
allocResourceRequest(phys_ptr<ResourceHandleManager> resourceHandleManager,
                     CpuID cpuID,
                     phys_ptr<ResourceManager> resourceManager,
                     phys_ptr<MessageQueue> messageQueue,
                     phys_ptr<IpcRequest> ipcRequest,
                     phys_ptr<ResourceRequest> *outResourceRequest);

static Error
freeResourceRequest(phys_ptr<ResourceRequest> resourceRequest);

static Error
allocResourceHandle(phys_ptr<ResourceHandleManager> resourceHandleManager,
                    phys_ptr<ResourceManager> resourceManager,
                    ResourceHandleID *outResourceHandleID);

static Error
freeResourceHandle(phys_ptr<ResourceHandleManager> resourceHandleManager,
                   ResourceHandleID id);

static Error
getResourceHandle(ResourceHandleID id,
                  phys_ptr<ResourceHandleManager> resourceHandleManager,
                  phys_ptr<ResourceHandle> *outResourceHandle);

static Error
dispatchResourceReply(phys_ptr<ResourceRequest> resourceRequest,
                      Error reply,
                      bool freeRequest);

static Error
dispatchRequest(phys_ptr<ResourceRequest> request);

} // namespace internal

/**
 * Register a message queue to receive messages for a device.
 */
Error
IOS_RegisterResourceManager(std::string_view device,
                            MessageQueueID queue)
{
   auto pid = internal::getCurrentProcessID();
   auto resourceHandleManager = internal::getResourceHandleManager(pid);
   if (!resourceHandleManager) {
      return Error::Invalid;
   }

   auto error = internal::findResourceManager(device, nullptr);
   if (error < Error::OK) {
      return Error::Exists;
   }

   auto &resourceManagerList = sData->resourceManagerList;
   auto resourceManagerIdx = resourceManagerList.firstFreeIdx;
   if (resourceHandleManager->numResourceManagers >= resourceHandleManager->maxResourceManagers ||
       resourceManagerIdx < 0) {
      return Error::FailAlloc;
   }

   auto &resourceManager = resourceManagerList.resourceManagers[resourceManagerIdx];
   auto nextFreeIdx = resourceManager.nextResourceManagerIdx;

   if (nextFreeIdx < 0) {
      resourceManagerList.firstFreeIdx = int16_t { -1 };
      resourceManagerList.lastFreeIdx = int16_t { -1 };
   } else {
      auto &nextFreeResourceManager = resourceManagerList.resourceManagers[nextFreeIdx];
      nextFreeResourceManager.prevResourceManagerIdx = int16_t { -1 };
      resourceManagerList.firstFreeIdx = nextFreeIdx;
   }

   resourceManager.queueId = queue;

   std::strncpy(phys_addrof(resourceManager.device).getRawPointer(), device.data(), resourceManager.device.size());
   resourceManager.deviceLen = static_cast<uint16_t>(device.size());

   resourceManager.numRequests = uint16_t { 0u };
   resourceManager.firstRequestIdx = int16_t { -1 };
   resourceManager.lastRequestIdx = int16_t { -1 };

   resourceManager.prevResourceManagerIdx = int16_t { -1 };
   resourceManager.nextResourceManagerIdx = int16_t { -1 };

   resourceManager.numHandles = uint16_t { 0u };
   resourceManager.unk0x3C = uint16_t { 8u };

   resourceManagerList.numRegistered++;
   if (resourceManagerList.numRegistered > resourceManagerList.mostRegistered) {
      resourceManagerList.mostRegistered = resourceManagerList.numRegistered;
   }

   resourceHandleManager->numResourceManagers++;
   resourceManager.resourceHandleManager = resourceHandleManager;

   if (resourceManagerList.firstRegisteredIdx < 0) {
      resourceManagerList.firstRegisteredIdx = resourceManagerIdx;
      resourceManagerList.lastRegisteredIdx = resourceManagerIdx;
   } else {
      // TODO: Insert ordered
   }

   return Error::OK;
}


/**
 * Register a device with a permission group (matching cos.xml)
 */
Error
IOS_SetResourcePermissionGroup(std::string_view device,
                               uint32_t group)
{
   phys_ptr<ResourceManager> resourceManager;
   auto pid = internal::getCurrentProcessID();
   auto error = internal::findResourceManager(device, &resourceManager);
   if (error < Error::OK) {
      return error;
   }

   if (pid != resourceManager->resourceHandleManager->pid) {
      return Error::Access;
   }

   resourceManager->permissionGroup = group;
   return Error::OK;
}


/**
 * Send a reply to a resource request.
 */
Error
IOS_ResourceReply(phys_ptr<ResourceRequest> resourceRequest,
                  Error reply)
{
   phys_ptr<ResourceHandle> resourceHandle;

   // Get the resource handle manager for the current process
   auto pid = internal::getCurrentProcessID();
   auto resourceHandleManager = internal::getResourceHandleManager(pid);
   if (!resourceHandleManager) {
      resourceHandleManager->failedRegisterMaxResourceRequests++;
      return Error::Invalid;
   }

   // Calculate the request index
   auto &resourceRequestList = sData->resourceRequestList;
   auto resourceRequestIndex = resourceRequest - phys_addrof(resourceRequestList.resourceRequests[0]);
   if (resourceRequestIndex < 0 ||
       resourceRequestIndex >= resourceRequestList.resourceRequests.size()) {
      resourceHandleManager->failedRegisterMaxResourceRequests++;
      return Error::Invalid;
   }

   if (resourceRequest != phys_addrof(resourceRequestList.resourceRequests[resourceRequestIndex]) ||
      resourceHandleManager != resourceRequest->resourceManager->resourceHandleManager) {
      resourceHandleManager->failedRegisterMaxResourceRequests++;
      return Error::Invalid;
   }

   auto error = internal::getResourceHandle(resourceRequest->resourceHandleID,
                                            resourceHandleManager,
                                            &resourceHandle);
   if (error < Error::OK) {
      gLog->warn("IOS_ResourceReply(0x{:08X}, {}) passed invalid resource request.",
                 phys_addr { resourceRequest },
                 reply);
      resourceHandleManager->failedRegisterMaxResourceRequests++;
   } else if (resourceRequest->requestData.command == Command::Open) {
      if (reply < Error::OK) {
         // Resource open failed, free the resource handle.
         internal::freeResourceHandle(resourceHandleManager,
                                      resourceRequest->resourceHandleID);
      } else {
         // Resource open succeeded, save the resource handle.
         resourceHandle->handle = static_cast<int32_t>(reply);
         resourceHandle->state = ResourceHandleState::Open;
         reply = static_cast<Error>(resourceHandle->id.value());
      }
   } else if (resourceRequest->requestData.command == Command::Close) {
      // Resource closed, close the resource handle.
      internal::freeResourceHandle(resourceHandleManager,
                                   resourceRequest->resourceHandleID);
   }

   return internal::dispatchResourceReply(resourceRequest, reply, true);
}


namespace internal
{


/**
 * Find a registered ResourceManager for the given device name.
 */
static Error
findResourceManager(std::string_view device,
                    phys_ptr<ResourceManager> *outResourceManager)
{
   auto index = sData->resourceManagerList.firstRegisteredIdx;
   while (index > 0) {
      auto &resourceManager = sData->resourceManagerList.resourceManagers[index];
      auto resourceManagerDevice = std::string_view {
            resourceManager.device.phys_begin().getRawPointer(),
            resourceManager.deviceLen
         };

      if (device.compare(resourceManagerDevice) == 0) {
         if (outResourceManager) {
            *outResourceManager = phys_addrof(resourceManager);
         }

         return Error::OK;
      }

      index = resourceManager.nextResourceManagerIdx;
   }

   return Error::NoExists;
}


/**
 * Get the ResourceHandleManager for the specified process.
 */
static phys_ptr<ResourceHandleManager>
getResourceHandleManager(ProcessID id)
{
   if (id >= ProcessID::Max) {
      return nullptr;
   }

   return phys_addrof(sData->resourceHandleManagers[id]);
}


/**
 * Allocate a ResourceRequest.
 */
static Error
allocResourceRequest(phys_ptr<ResourceHandleManager> resourceHandleManager,
                     CpuID cpuID,
                     phys_ptr<ResourceManager> resourceManager,
                     phys_ptr<MessageQueue> messageQueue,
                     phys_ptr<IpcRequest> ipcRequest,
                     phys_ptr<ResourceRequest> *outResourceRequest)
{
   if (resourceHandleManager->numResourceRequests >= resourceHandleManager->maxResourceRequests) {
      return Error::ClientTxnLimit;
   }

   // Find a free resource request to allocate.
   auto &resourceRequestList = sData->resourceRequestList;
   if (resourceRequestList.firstFreeIdx < 0) {
      return Error::FailAlloc;
   }

   auto resourceRequestIdx = resourceRequestList.firstFreeIdx;
   auto resourceRequest = phys_addrof(resourceRequestList.resourceRequests[resourceRequestIdx]);

   auto nextFreeIdx = resourceRequest->nextIdx;
   if (nextFreeIdx < 0) {
      resourceRequestList.firstFreeIdx = int16_t { -1 };
      resourceRequestList.lastFreeIdx = int16_t { -1 };
   } else {
      auto &nextFreeResourceRequest = resourceRequestList.resourceRequests[nextFreeIdx];
      nextFreeResourceRequest.nextIdx = int16_t { -1 };
      resourceRequestList.firstFreeIdx = nextFreeIdx;
   }

   resourceRequest->resourceHandleID = static_cast<ResourceHandleID>(Error::Invalid);
   resourceRequest->messageQueueId = messageQueue->uid;
   resourceRequest->messageQueue = messageQueue;
   resourceRequest->resourceHandleManager = resourceHandleManager;
   resourceRequest->resourceManager = resourceManager;
   resourceRequest->ipcRequest = ipcRequest;

   resourceRequest->requestData.cpuID = cpuID;
   resourceRequest->requestData.clientPid = resourceHandleManager->pid;
   resourceRequest->requestData.titleId = resourceHandleManager->tid;
   resourceRequest->requestData.gid = resourceHandleManager->gid;

   // Insert into the allocated request list.
   resourceRequest->nextIdx = int16_t { -1 };
   resourceRequest->prevIdx = resourceManager->lastRequestIdx;

   if (resourceManager->lastRequestIdx < 0) {
      resourceManager->firstRequestIdx = resourceRequestIdx;
      resourceManager->lastRequestIdx = resourceRequestIdx;
   } else {
      auto &lastRequest = resourceRequestList.resourceRequests[resourceManager->lastRequestIdx];
      lastRequest.nextIdx = resourceRequestIdx;
      resourceManager->lastRequestIdx = resourceRequestIdx;
   }

   // Increment our counters!
   resourceManager->numRequests++;

   resourceRequestList.numRegistered++;
   if (resourceRequestList.numRegistered > resourceRequestList.mostRegistered) {
      resourceRequestList.mostRegistered = resourceRequestList.numRegistered;
   }

   resourceHandleManager->numResourceRequests++;
   if (resourceHandleManager->numResourceRequests > resourceHandleManager->mostResourceRequests) {
      resourceHandleManager->mostResourceRequests = resourceHandleManager->numResourceRequests;
   }

   *outResourceRequest = resourceRequest;
   return Error::OK;
}


/**
 * Free a ResourceRequest.
 */
static Error
freeResourceRequest(phys_ptr<ResourceRequest> resourceRequest)
{
   auto &resourceRequestList = sData->resourceRequestList;
   auto resourceManager = resourceRequest->resourceManager;

   if (!resourceManager) {
      return Error::Invalid;
   }

   // Remove from the request list
   auto resourceRequestIndex = static_cast<int16_t>(resourceRequest - phys_addrof(resourceRequestList.resourceRequests[0]));
   auto lastFreeIdx = resourceRequestList.lastFreeIdx;
   auto nextIdx = resourceRequest->nextIdx;
   auto prevIdx = resourceRequest->prevIdx;

   if (nextIdx >= 0) {
      auto &nextResourceRequest = resourceRequestList.resourceRequests[nextIdx];
      nextResourceRequest.prevIdx = prevIdx;
   }

   if (prevIdx >= 0) {
      auto &prevResourceRequest = resourceRequestList.resourceRequests[prevIdx];
      prevResourceRequest.nextIdx = nextIdx;
   }

   if (resourceManager->firstRequestIdx == resourceRequestIndex) {
      resourceManager->firstRequestIdx = nextIdx;
   }

   if (resourceManager->lastRequestIdx == resourceRequestIndex) {
      resourceManager->lastRequestIdx = prevIdx;
   }

   // Decrement our counters!
   auto resourceHandleManager = resourceManager->resourceHandleManager;
   resourceHandleManager->numResourceRequests--;
   resourceRequestList.numRegistered--;
   resourceManager->numRequests--;

   // Reset the resource request
   std::memset(resourceRequest.getRawPointer(), 0, sizeof(ResourceRequest));
   resourceRequest->prevIdx = lastFreeIdx;
   resourceRequest->nextIdx = int16_t { -1 };

   // Reinsert into free list
   resourceRequestList.lastFreeIdx = resourceRequestIndex;

   if (lastFreeIdx <= 0) {
      resourceRequestList.firstFreeIdx = resourceRequestIndex;
   } else {
      auto &lastFreeResourceRequest = resourceRequestList.resourceRequests[lastFreeIdx];
      lastFreeResourceRequest.nextIdx = resourceRequestIndex;
   }

   return Error::OK;
}


/**
 * Allocate a ResourceHandle.
 */
static Error
allocResourceHandle(phys_ptr<ResourceHandleManager> resourceHandleManager,
                    phys_ptr<ResourceManager> resourceManager,
                    ResourceHandleID *outResourceHandleID)
{
   // Check if we have a free resource handle to register.
   if (resourceHandleManager->numResourceHandles >= resourceHandleManager->maxResourceHandles) {
      return Error::Max;
   }

   // Find a free resource handle
   phys_ptr<ResourceHandle> resourceHandle = nullptr;
   auto resourceHandleIdx = 0u;

   for (auto i = 0u; i < resourceHandleManager->handles.size(); ++i) {
      if (resourceHandleManager->handles[i].state == ResourceHandleState::Free) {
         resourceHandle = phys_addrof(resourceHandleManager->handles);
         resourceHandleIdx = i;
         break;
      }
   }

   // Double check we have one... should never happen really.
   if (!resourceHandle) {
      return Error::Max;
   }

   resourceHandle->id = static_cast<ResourceHandleID>(resourceHandleIdx | (sData->totalOpenedHandles << 12));
   resourceHandle->resourceManager = resourceManager;
   resourceHandle->state = ResourceHandleState::Opening;
   resourceHandle->handle = -10;

   *outResourceHandleID = resourceHandle->id;
   return Error::OK;
}


/**
 * Free a ResourceHandle.
 */
static Error
freeResourceHandle(phys_ptr<ResourceHandleManager> resourceHandleManager,
                   ResourceHandleID id)
{
   phys_ptr<ResourceHandle> resourceHandle;
   auto error = getResourceHandle(id, resourceHandleManager, &resourceHandle);
   if (error < Error::OK) {
      return error;
   }

   auto resourceManager = resourceHandle->resourceManager;
   resourceHandle->handle = -4;
   resourceHandle->resourceManager = nullptr;
   resourceHandle->state = ResourceHandleState::Free;
   resourceHandle->id = static_cast<ResourceHandleID>(Error::Invalid);

   resourceHandleManager->numResourceHandles--;
   resourceManager->numHandles--;
   return Error::OK;
}


/**
 * Get ResourceHandle by id.
 */
static Error
getResourceHandle(ResourceHandleID id,
                  phys_ptr<ResourceHandleManager> resourceHandleManager,
                  phys_ptr<ResourceHandle> *outResourceHandle)
{
   if (id >= 0) {
      id &= 0xFFF;
   }

   if (id >= resourceHandleManager->handles.size()) {
      return Error::Invalid;
   }

   if (resourceHandleManager->handles[id].id != id) {
      return Error::NoExists;
   }

   *outResourceHandle = phys_addrof(resourceHandleManager->handles[id]);
   return Error::OK;
}


/**
 * Lookup an open resource handle.
 */
static Error
getOpenResource(ProcessID pid,
                ResourceHandleID id,
                phys_ptr<ResourceHandleManager> *outResourceHandleManager,
                phys_ptr<ResourceHandle> *outResourceHandle)
{
   phys_ptr<ResourceHandle> resourceHandle;

   // Try get the resource handle manager for this process.
   auto resourceHandleManager = getResourceHandleManager(pid);
   if (!resourceHandleManager) {
      return Error::Invalid;
   }

   auto error = getResourceHandle(id, resourceHandleManager, &resourceHandle);
   if (resourceHandle->state != ResourceHandleState::Closed) {
      return Error::StaleHandle;
   } else if (resourceHandle->state != ResourceHandleState::Open) {
      return Error::Invalid;
   }

   *outResourceHandle = resourceHandle;
   *outResourceHandleManager = resourceHandleManager;
   return Error::OK;
}


/**
 * Dispatch a reply to a ResourceRequest.
 */
static Error
dispatchResourceReply(phys_ptr<ResourceRequest> resourceRequest,
                      Error reply,
                      bool freeRequest)
{
   auto error = Error::Invalid;
   phys_ptr<IpcRequest> ipcRequest = resourceRequest->ipcRequest;
   ipcRequest->command = Command::Reply;
   ipcRequest->reply = reply;

   if (resourceRequest->messageQueueId == getIpcMessageQueueID()) {
      ::kernel::ipcDriverKernelSubmitReply(ipcRequest.getRawPointer());
      error = Error::OK;
   } else {
      phys_ptr<MessageQueue> queue = nullptr;

      if (resourceRequest->messageQueueId < 0) {
         queue = resourceRequest->messageQueue;
      } else {
         queue = getMessageQueue(resourceRequest->messageQueueId);
      }

      if (queue) {
         error = sendMessage(queue,
                             static_cast<Message>(phys_addr { ipcRequest }.getAddress()),
                             MessageFlags::NonBlocking);
      }
   }

   if (freeRequest) {
      freeResourceRequest(resourceRequest);
   }

   return error;
}


/**
 * Dispatch a request to it's message queue.
 */
static Error
dispatchRequest(phys_ptr<ResourceRequest> request)
{
   return IOS_SendMessage(request->messageQueueId,
                          static_cast<Message>(phys_addr { request }.getAddress()),
                          MessageFlags::NonBlocking);

}


/**
 * Dispatch an IOS_Open request.
 */
Error
dispatchIosOpen(std::string_view device,
                OpenMode mode,
                phys_ptr<MessageQueue> queue,
                phys_ptr<IpcRequest> ipcRequest,
                ProcessID pid,
                CpuID cpuID)
{
   phys_ptr<ResourceManager> resourceManager;
   phys_ptr<ResourceRequest> resourceRequest;
   ResourceHandleID resourceHandleID;

   // Try get the resource handle manager for this process.
   auto resourceHandleManager = internal::getResourceHandleManager(pid);
   if (!resourceHandleManager) {
      return Error::Invalid;
   }

   // Try find the resource manager for this device name.
   auto error = internal::findResourceManager(device, &resourceManager);
   if (error < Error::OK) {
      return error;
   }

   // Try allocate a resource request.
   error = allocResourceRequest(resourceHandleManager,
                                cpuID,
                                resourceManager,
                                queue,
                                ipcRequest,
                                &resourceRequest);
   if (error < Error::OK) {
      return error;
   }

   resourceRequest->requestData.command = Command::Open;

   // Set the IOS_Open arguments
   resourceRequest->requestData.args.open.name = phys_addrof(resourceRequest->openNameBuffer);
   resourceRequest->requestData.args.open.mode = mode;

   std::strncpy(resourceRequest->openNameBuffer.phys_begin().getRawPointer(),
                device.data(),
                resourceRequest->openNameBuffer.size());

   // Try allocate a resource handle.
   error = allocResourceHandle(resourceHandleManager,
                               resourceManager,
                               &resourceHandleID);
   if (error < Error::OK) {
      freeResourceRequest(resourceRequest);
      return error;
   }

   resourceRequest->resourceHandleID = resourceHandleID;

   // Increment our counters!
   sData->totalOpenedHandles++;
   resourceManager->numHandles++;
   resourceHandleManager->numResourceHandles++;

   // Try dispatch the request to the relevant resource manager.
   error = dispatchRequest(resourceRequest);
   if (error < Error::OK) {
      freeResourceHandle(resourceHandleManager, resourceHandleID);
      freeResourceRequest(resourceRequest);
      return error;
   }

   return Error::OK;
}


/**
 * Dispatch an IOS_Close request.
 */
Error
dispatchIosClose(ResourceHandleID resourceHandleID,
                 phys_ptr<MessageQueue> queue,
                 phys_ptr<IpcRequest> ipcRequest,
                 uint32_t unkArg0,
                 ProcessID pid,
                 CpuID cpuID)
{
   phys_ptr<ResourceHandle> resourceHandle;
   phys_ptr<ResourceRequest> resourceRequest;

   // Try get the resource handle manager for this process.
   auto resourceHandleManager = getResourceHandleManager(pid);
   if (!resourceHandleManager) {
      return Error::Invalid;
   }

   // Try lookup the resource handle.
   auto error = getResourceHandle(resourceHandleID,
                                  resourceHandleManager,
                                  &resourceHandle);
   if (error < Error::OK) {
      return error;
   }

   // If the handle no longer has a resource manager associated with it then
   // we need to hack in a resource request.
   auto resourceManager = resourceHandle->resourceManager;
   if (!resourceManager) {
      ios::StackObject<ResourceRequest> resourceRequest;
      std::memset(resourceRequest.getRawPointer(), 0, sizeof(ResourceRequest));
      resourceRequest->requestData.command = Command::Close;
      resourceRequest->requestData.cpuID = cpuID;
      resourceRequest->requestData.clientPid = pid;
      resourceRequest->requestData.handle = resourceHandleID;
      resourceRequest->ipcRequest = ipcRequest;
      resourceRequest->messageQueue = queue;
      resourceRequest->messageQueueId = queue->uid;

      freeResourceHandle(resourceHandleManager, resourceHandleID);
      return dispatchRequest(resourceRequest);
   }

   // Try allocate a resource request.
   error = allocResourceRequest(resourceHandleManager,
                                cpuID,
                                resourceManager,
                                queue,
                                ipcRequest,
                                &resourceRequest);
   if (error < Error::OK) {
      return error;
   }

   resourceRequest->requestData.command = Command::Close;
   resourceRequest->resourceHandleID = resourceHandleID;
   resourceRequest->requestData.handle = resourceHandle->handle;
   resourceRequest->requestData.args.close.unk0x00 = unkArg0;

   auto previousResourceHandleState = resourceHandle->state;
   resourceHandle->state = ResourceHandleState::Closed;

   // Try dispatch the request to the relevant resource manager.
   error = dispatchRequest(resourceRequest);
   if (error < Error::OK) {
      resourceHandle->state = previousResourceHandleState;
      freeResourceRequest(resourceRequest);
      return error;
   }

   return Error::OK;
}


/**
 * Dispatch an IOS_Read request.
 */
Error
dispatchIosRead(ResourceHandleID resourceHandleID,
                phys_ptr<void> buffer,
                uint32_t length,
                phys_ptr<MessageQueue> queue,
                phys_ptr<IpcRequest> ipcRequest,
                ProcessID pid,
                CpuID cpuID)
{
   phys_ptr<ResourceHandle> resourceHandle;
   phys_ptr<ResourceHandleManager> resourceHandleManager;
   phys_ptr<ResourceRequest> resourceRequest;

   auto error = getOpenResource(pid, resourceHandleID, &resourceHandleManager, &resourceHandle);
   if (error < Error::OK) {
      return error;
   }

   if (length && !buffer) {
      return Error::Access;
   }

   // Try allocate a resource request.
   error = allocResourceRequest(resourceHandleManager,
                                cpuID,
                                resourceHandle->resourceManager,
                                queue,
                                ipcRequest,
                                &resourceRequest);
   if (error < Error::OK) {
      return error;
   }

   resourceRequest->requestData.command = Command::Read;
   resourceRequest->resourceHandleID = resourceHandleID;
   resourceRequest->requestData.handle = resourceHandle->handle;
   resourceRequest->requestData.args.read.data = buffer;
   resourceRequest->requestData.args.read.length = length;

   // Try dispatch the request to the relevant resource manager.
   error = dispatchRequest(resourceRequest);
   if (error < Error::OK) {
      freeResourceRequest(resourceRequest);
      return error;
   }

   return Error::OK;
}


/**
 * Dispatch an IOS_Write request.
 */
Error
dispatchIosWrite(ResourceHandleID resourceHandleID,
                 phys_ptr<const void> buffer,
                 uint32_t length,
                 phys_ptr<MessageQueue> queue,
                 phys_ptr<IpcRequest> ipcRequest,
                 ProcessID pid,
                 CpuID cpuID)
{
   phys_ptr<ResourceHandle> resourceHandle;
   phys_ptr<ResourceHandleManager> resourceHandleManager;
   phys_ptr<ResourceRequest> resourceRequest;

   auto error = getOpenResource(pid,
                                resourceHandleID,
                                &resourceHandleManager,
                                &resourceHandle);
   if (error < Error::OK) {
      return error;
   }

   if (length && !buffer) {
      return Error::Access;
   }

   // Try allocate a resource request.
   auto error = allocResourceRequest(resourceHandleManager,
                                     cpuID,
                                     resourceHandle->resourceManager,
                                     queue,
                                     ipcRequest,
                                     &resourceRequest);
   if (error < Error::OK) {
      return error;
   }

   resourceRequest->requestData.command = Command::Write;
   resourceRequest->resourceHandleID = resourceHandleID;
   resourceRequest->requestData.handle = resourceHandle->handle;
   resourceRequest->requestData.args.write.data = buffer;
   resourceRequest->requestData.args.write.length = length;

   // Try dispatch the request to the relevant resource manager.
   error = dispatchRequest(resourceRequest);
   if (error < Error::OK) {
      freeResourceRequest(resourceRequest);
      return error;
   }

   return Error::OK;
}


/**
 * Dispatch an IOS_Seek request.
 */
Error
dispatchIosSeek(ResourceHandleID resourceHandleID,
                uint32_t offset,
                SeekOrigin origin,
                phys_ptr<MessageQueue> queue,
                phys_ptr<IpcRequest> ipcRequest,
                ProcessID pid,
                CpuID cpuID)
{
   phys_ptr<ResourceHandle> resourceHandle;
   phys_ptr<ResourceHandleManager> resourceHandleManager;
   phys_ptr<ResourceRequest> resourceRequest;

   auto error = getOpenResource(pid,
                                resourceHandleID,
                                &resourceHandleManager,
                                &resourceHandle);
   if (error < Error::OK) {
      return error;
   }

   // Try allocate a resource request.
   error = allocResourceRequest(resourceHandleManager,
                                cpuID,
                                resourceHandle->resourceManager,
                                queue,
                                ipcRequest,
                                &resourceRequest);
   if (error < Error::OK) {
      return error;
   }

   resourceRequest->requestData.command = Command::Seek;
   resourceRequest->resourceHandleID = resourceHandleID;
   resourceRequest->requestData.handle = resourceHandle->handle;
   resourceRequest->requestData.args.seek.offset = offset;
   resourceRequest->requestData.args.seek.origin = origin;

   // Try dispatch the request to the relevant resource manager.
   error = dispatchRequest(resourceRequest);
   if (error < Error::OK) {
      freeResourceRequest(resourceRequest);
      return error;
   }

   return Error::OK;
}


/**
 * Dispatch an IOS_Ioctl request.
 */
Error
dispatchIosIoctl(ResourceHandleID resourceHandleID,
                 uint32_t ioctlRequest,
                 phys_ptr<void> inputBuffer,
                 uint32_t inputLength,
                 phys_ptr<void> outputBuffer,
                 uint32_t outputLength,
                 phys_ptr<MessageQueue> queue,
                 phys_ptr<IpcRequest> ipcRequest,
                 ProcessID pid,
                 CpuID cpuID)
{
   phys_ptr<ResourceHandle> resourceHandle;
   phys_ptr<ResourceHandleManager> resourceHandleManager;
   phys_ptr<ResourceRequest> resourceRequest;

   auto error = getOpenResource(pid,
                                resourceHandleID,
                                &resourceHandleManager,
                                &resourceHandle);
   if (error < Error::OK) {
      return error;
   }

   if ((inputLength && !inputBuffer) || (outputLength && !outputBuffer)) {
      return Error::Access;
   }

   // Try allocate a resource request.
   error = allocResourceRequest(resourceHandleManager,
                                cpuID,
                                resourceHandle->resourceManager,
                                queue,
                                ipcRequest,
                                &resourceRequest);
   if (error < Error::OK) {
      return error;
   }

   resourceRequest->requestData.command = Command::Ioctl;
   resourceRequest->resourceHandleID = resourceHandleID;
   resourceRequest->requestData.handle = resourceHandle->handle;
   resourceRequest->requestData.args.ioctl.request = ioctlRequest;
   resourceRequest->requestData.args.ioctl.inputBuffer = inputBuffer;
   resourceRequest->requestData.args.ioctl.inputLength = inputLength;
   resourceRequest->requestData.args.ioctl.outputBuffer = outputBuffer;
   resourceRequest->requestData.args.ioctl.outputLength = outputLength;

   // Try dispatch the request to the relevant resource manager.
   error = dispatchRequest(resourceRequest);
   if (error < Error::OK) {
      freeResourceRequest(resourceRequest);
      return error;
   }

   return Error::OK;
}


/**
 * Dispatch an IOS_Ioctlv request.
 */
Error
dispatchIosIoctlv(ResourceHandleID resourceHandleID,
                  uint32_t ioctlRequest,
                  uint32_t numVecIn,
                  uint32_t numVecOut,
                  phys_ptr<IoctlVec> vecs,
                  phys_ptr<MessageQueue> queue,
                  phys_ptr<IpcRequest> ipcRequest,
                  ProcessID pid,
                  CpuID cpuID)
{
   phys_ptr<ResourceHandle> resourceHandle;
   phys_ptr<ResourceHandleManager> resourceHandleManager;
   phys_ptr<ResourceRequest> resourceRequest;

   auto error = getOpenResource(pid,
                                resourceHandleID,
                                &resourceHandleManager,
                                &resourceHandle);
   if (error < Error::OK) {
      return error;
   }

   if (numVecIn + numVecOut > 0 && !vecs) {
      return Error::Access;
   }

   // Try allocate a resource request.
   error = allocResourceRequest(resourceHandleManager,
                                cpuID,
                                resourceHandle->resourceManager,
                                queue,
                                ipcRequest,
                                &resourceRequest);
   if (error < Error::OK) {
      return error;
   }

   resourceRequest->requestData.command = Command::Ioctlv;
   resourceRequest->resourceHandleID = resourceHandleID;
   resourceRequest->requestData.handle = resourceHandle->handle;
   resourceRequest->requestData.args.ioctlv.request = ioctlRequest;
   resourceRequest->requestData.args.ioctlv.numVecIn = numVecIn;
   resourceRequest->requestData.args.ioctlv.numVecOut = numVecOut;
   resourceRequest->requestData.args.ioctlv.vecs = vecs;

   // Try dispatch the request to the relevant resource manager.
   error = dispatchRequest(resourceRequest);
   if (error < Error::OK) {
      freeResourceRequest(resourceRequest);
      return error;
   }

   return Error::OK;
}


/**
* Dispatch an IOS_Resume request.
*/
Error
dispatchIosResume(ResourceHandleID resourceHandleID,
                  uint32_t unkArg0,
                  uint32_t unkArg1,
                  phys_ptr<MessageQueue> queue,
                  phys_ptr<IpcRequest> ipcRequest,
                  ProcessID pid,
                  CpuID cpuID)
{
   phys_ptr<ResourceHandle> resourceHandle;
   phys_ptr<ResourceHandleManager> resourceHandleManager;
   phys_ptr<ResourceRequest> resourceRequest;

   auto error = getOpenResource(pid,
                                resourceHandleID,
                                &resourceHandleManager,
                                &resourceHandle);
   if (error < Error::OK) {
      return error;
   }

   // Try allocate a resource request.
   error = allocResourceRequest(resourceHandleManager,
                                cpuID,
                                resourceHandle->resourceManager,
                                queue,
                                ipcRequest,
                                &resourceRequest);
   if (error < Error::OK) {
      return error;
   }

   resourceRequest->requestData.command = Command::Resume;
   resourceRequest->resourceHandleID = resourceHandleID;
   resourceRequest->requestData.handle = resourceHandle->handle;
   resourceRequest->requestData.args.resume.unkArg0 = unkArg0;
   resourceRequest->requestData.args.resume.unkArg1 = unkArg1;

   // Try dispatch the request to the relevant resource manager.
   error = dispatchRequest(resourceRequest);
   if (error < Error::OK) {
      freeResourceRequest(resourceRequest);
      return error;
   }

   return Error::OK;
}


/**
 * Dispatch an IOS_Suspend request.
 */
Error
dispatchIosSuspend(ResourceHandleID resourceHandleID,
                   uint32_t unkArg0,
                   uint32_t unkArg1,
                   phys_ptr<MessageQueue> queue,
                   phys_ptr<IpcRequest> ipcRequest,
                   ProcessID pid,
                   CpuID cpuID)
{
   phys_ptr<ResourceHandle> resourceHandle;
   phys_ptr<ResourceHandleManager> resourceHandleManager;
   phys_ptr<ResourceRequest> resourceRequest;

   auto error = getOpenResource(pid,
                                resourceHandleID,
                                &resourceHandleManager,
                                &resourceHandle);
   if (error < Error::OK) {
      return error;
   }

   // Try allocate a resource request.
   error = allocResourceRequest(resourceHandleManager,
                                cpuID,
                                resourceHandle->resourceManager,
                                queue,
                                ipcRequest,
                                &resourceRequest);
   if (error < Error::OK) {
      return error;
   }

   resourceRequest->requestData.command = Command::Suspend;
   resourceRequest->resourceHandleID = resourceHandleID;
   resourceRequest->requestData.handle = resourceHandle->handle;
   resourceRequest->requestData.args.suspend.unkArg0 = unkArg0;
   resourceRequest->requestData.args.suspend.unkArg1 = unkArg1;

   // Try dispatch the request to the relevant resource manager.
   error = dispatchRequest(resourceRequest);
   if (error < Error::OK) {
      freeResourceRequest(resourceRequest);
      return error;
   }

   return Error::OK;
}


/**
 * Dispatch an IOS_SvcMsg request.
 */
Error
dispatchIosSvcMsg(ResourceHandleID resourceHandleID,
                  uint32_t unkArg0,
                  uint32_t unkArg1,
                  uint32_t unkArg2,
                  uint32_t unkArg3,
                  phys_ptr<MessageQueue> queue,
                  phys_ptr<IpcRequest> ipcRequest,
                  ProcessID pid,
                  CpuID cpuID)
{
   phys_ptr<ResourceHandle> resourceHandle;
   phys_ptr<ResourceHandleManager> resourceHandleManager;
   phys_ptr<ResourceRequest> resourceRequest;

   auto error = getOpenResource(pid,
                                resourceHandleID,
                                &resourceHandleManager,
                                &resourceHandle);
   if (error < Error::OK) {
      return error;
   }

   // Try allocate a resource request.
   error = allocResourceRequest(resourceHandleManager,
                                cpuID,
                                resourceHandle->resourceManager,
                                queue,
                                ipcRequest,
                                &resourceRequest);
   if (error < Error::OK) {
      return error;
   }

   resourceRequest->requestData.command = Command::SvcMsg;
   resourceRequest->resourceHandleID = resourceHandleID;
   resourceRequest->requestData.handle = resourceHandle->handle;
   resourceRequest->requestData.args.svcMsg.unkArg0 = unkArg0;
   resourceRequest->requestData.args.svcMsg.unkArg1 = unkArg1;
   resourceRequest->requestData.args.svcMsg.unkArg2 = unkArg2;
   resourceRequest->requestData.args.svcMsg.unkArg3 = unkArg3;

   // Try dispatch the request to the relevant resource manager.
   error = dispatchRequest(resourceRequest);
   if (error < Error::OK) {
      freeResourceRequest(resourceRequest);
      return error;
   }

   return Error::OK;
}


/**
 * Find the ClientCapability structure for a specific feature ID.
 */
Error
getClientCapability(phys_ptr<ResourceHandleManager> resourceHandleManager,
                    FeatureID featureID,
                    phys_ptr<ClientCapability> *outClientCapability)
{
   for (auto i = 0u; i < resourceHandleManager->clientCapabilities.size(); ++i) {
      auto caps = phys_addrof(resourceHandleManager->clientCapabilities[i]);
      if (caps->featureID == featureID) {
         if (outClientCapability) {
            *outClientCapability = caps;
         }

         return Error::OK;
      }
   }

   return Error::NoExists;
}


/**
 * Set the client capability mask for a specific process & feature ID.
 */
Error
setClientCapability(ProcessID pid,
                    FeatureID featureID,
                    uint64_t mask)
{
   phys_ptr<ClientCapability> clientCapability;

   auto resourceHandleManager = getResourceHandleManager(pid);
   if (!resourceHandleManager) {
      return Error::InvalidArg;
   }

   auto error = getClientCapability(resourceHandleManager,
                                    featureID,
                                    &clientCapability);
   if (error >= Error::OK) {
      if (mask == 0) {
         // Delete client cap
         clientCapability->featureID = -1;
         clientCapability->mask = 0ull;
         return Error::OK;
      }

      // Update client cap
      clientCapability->mask = mask;
      return Error::OK;
   }

   if (mask == 0) {
      return Error::OK;
   }

   // Add new client cap
   clientCapability = nullptr;

   for (auto i = 0u; i < resourceHandleManager->clientCapabilities.size(); ++i) {
      auto cap = phys_addrof(resourceHandleManager->clientCapabilities[i]);

      if (cap->featureID == -1) {
         clientCapability = cap;
         break;
      }
   }

   if (!clientCapability) {
      return Error::FailAlloc;
   }

   clientCapability->featureID = featureID;
   clientCapability->mask = mask;
   return Error::OK;
}

void
kernelInitialiseResourceManager()
{
   // TODO: Allocate & zero sData

   // Initialise resourceManagerList
   auto &resourceManagerList = sData->resourceManagerList;
   resourceManagerList.firstRegisteredIdx = int16_t { -1 };
   resourceManagerList.lastRegisteredIdx = int16_t { -1 };

   resourceManagerList.firstFreeIdx = int16_t { 0 };
   resourceManagerList.lastFreeIdx = static_cast<int16_t>(MaxNumResourceManagers - 1);

   for (auto i = 0; i < MaxNumResourceManagers; ++i) {
      auto &resourceManager = resourceManagerList.resourceManagers[i];
      resourceManager.prevResourceManagerIdx = static_cast<int16_t>(i - 1);
      resourceManager.nextResourceManagerIdx = static_cast<int16_t>(i + 1);
   }

   resourceManagerList.resourceManagers[resourceManagerList.firstFreeIdx].prevResourceManagerIdx = int16_t { -1 };
   resourceManagerList.resourceManagers[resourceManagerList.lastFreeIdx].nextResourceManagerIdx = int16_t { -1 };

   // Initialise resourceRequestList
   auto &resourceRequestList = sData->resourceRequestList;
   resourceRequestList.firstFreeIdx = int16_t { 0 };
   resourceRequestList.lastFreeIdx = static_cast<int16_t>(MaxNumResourceRequests - 1);

   for (auto i = 0; i < MaxNumResourceRequests; ++i) {
      auto &resourceRequest = resourceRequestList.resourceRequests[i];
      resourceRequest.prevIdx = static_cast<int16_t>(i - 1);
      resourceRequest.nextIdx = static_cast<int16_t>(i + 1);
   }

   resourceRequestList.resourceRequests[resourceRequestList.firstFreeIdx].prevIdx = int16_t { -1 };
   resourceRequestList.resourceRequests[resourceRequestList.lastFreeIdx].nextIdx = int16_t { -1 };

   // Initialise resourceHandleManagers
   auto &resourceHandleManagers = sData->resourceHandleManagers;
   for (auto i = 0u; i < resourceHandleManagers.size(); ++i) {
      auto &resourceHandleManager = resourceHandleManagers[i];

      resourceHandleManager.pid = static_cast<ProcessID>(i);
      resourceHandleManager.maxResourceHandles = MaxNumResourceHandlesPerProcess;
      resourceHandleManager.maxResourceRequests = MaxNumResourceRequestsPerProcess;

      if (resourceHandleManager.pid >= ProcessID::COSKERNEL) {
         resourceHandleManager.maxResourceManagers = 0u;
      } else {
         resourceHandleManager.maxResourceManagers = MaxNumResourceManagersPerProcess;
      }

      for (auto j = 0u; j < MaxNumResourceHandlesPerProcess; ++j) {
         auto &handle = resourceHandleManager.handles[j];
         handle.handle = -4;
         handle.id = -4;
         handle.resourceManager = nullptr;
         handle.state = ResourceHandleState::Free;
      }

      for (auto j = 0u; j < MaxNumClientCapabilitiesPerProcess; ++j) {
         auto &caps = resourceHandleManager.clientCapabilities[j];
         caps.featureID = -1;
         caps.mask = 0ull;
      }

      setClientCapability(resourceHandleManager.pid, 0, 0xFFFFFFFFFFFFFFFFull);
   }
}

} // namespace internal

} // namespace ios

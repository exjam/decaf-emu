#include "ios_fs_fsa_device.h"
#include "ios_fs_mutex.h"

#include "ios/kernel/ios_kernel_messagequeue.h"
#include "ios/kernel/ios_kernel_resourcemanager.h"
#include "ios/kernel/ios_kernel_thread.h"
#include "ios/ios_enum.h"
#include "ios/ios_ipc.h"
#include "ios/ios_stackobject.h"

namespace ios::fs::internal
{

using FSADeviceHandle = int32_t;
constexpr auto FSAMaxClients = 0x270;

struct RequestOrigin
{
   be2_val<TitleId> titleId;
   be2_val<ProcessId> processId;
   be2_val<GroupId> groupId;
};

struct FsaPerProcessData
{
   be2_val<BOOL> initialised;
   be2_struct<RequestOrigin> origin;
   UNKNOWN(4);
   be2_val<kernel::ClientCapabilityMask> clientCapabilityMask;
   UNKNOWN(0x34 - 0x20);
   be2_val<uint32_t> numFilesOpen;
   UNKNOWN(0x4538 - 0x38);
   be2_array<char, FSAPathLength> unkPath0x4538;
   be2_array<char, FSAPathLength> unkPath0x47B8;
   be2_struct<Mutex> mutex;
};

struct FsaDeviceData
{
   be2_val<BOOL> initialised;
   be2_phys_ptr<FsaPerProcessData> perProcessData;
   UNKNOWN(0xAC - 0x08);
   be2_array<char, FSAPathLength> workingPath;
   UNKNOWN(0x4);
};

struct FsaData
{
   be2_val<kernel::MessageQueueId> fsaMessageQueue;
   be2_array<kernel::Message, 0x160> fsaMessageBuffer;

   be2_val<kernel::ThreadId> fsaThread;
   be2_array<uint8_t, 0x4000> fsaThreadStack;
};

static phys_ptr<FsaData>
sData = nullptr;

static std::vector<std::unique_ptr<FSADevice>>
sDevices;

FSADevice *
getDevice(FSADeviceHandle handle)
{
   if (handle < 0 || handle >= sDevices.size()) {
      return nullptr;
   }

   return sDevices[handle].get();
}

static FSAStatus
fsaDeviceOpen(phys_ptr<RequestOrigin> origin,
              FSADeviceHandle *outFsaHandle,
              kernel::ClientCapabilityMask clientCapabilityMask)
{
   auto handle = FSADeviceHandle { -1 };

   for (auto i = 0u; i < sDevices.size(); ++i) {
      if (!sDevices[i]) {
         handle = static_cast<FSADeviceHandle>(i);
         break;
      }
   }

   if (handle < 0) {
      if (sDevices.size() >= FSAMaxClients) {
         return FSAStatus::MaxClients;
      }

      handle = static_cast<FSADeviceHandle>(sDevices.size());
      sDevices.emplace_back(std::make_unique<FSADevice>());
   } else {
      sDevices[handle] = std::make_unique<FSADevice>();
   }

   *outFsaHandle = handle;
   return FSAStatus::OK;
}

static FSAStatus
fsaDeviceClose(FSADeviceHandle handle,
               phys_ptr<kernel::ResourceRequest> resourceRequest)
{
   auto device = getDevice(handle);
   if (!device) {
      return FSAStatus::InvalidClientHandle;
   }

   // TODO: Handle cleanup of async operations

   sDevices[handle] = nullptr;
   return FSAStatus::OK;
}

static FSAStatus
fsaDeviceIoctl(phys_ptr<kernel::ResourceRequest> resourceRequest,
               FSACommand command,
               be2_phys_ptr<const void> inputBuffer,
               be2_phys_ptr<void> outputBuffer)
{
   auto status = FSAStatus::OK;
   auto device = getDevice(resourceRequest->requestData.handle);
   auto request = phys_cast<const FSARequest>(inputBuffer);
   auto response = phys_cast<FSAResponse>(outputBuffer);

   if (!device) {
     status = FSAStatus::InvalidClientHandle;
   } else if (!inputBuffer && !outputBuffer) {
     status = FSAStatus::InvalidParam;
   } else if (request->emulatedError < 0) {
      return request->emulatedError;
   } else {
      switch (command) {
      case FSACommand::ChangeDir:
         status = device->changeDir(phys_addrof(request->changeDir));
         break;
      case FSACommand::CloseDir:
         status = device->closeDir(phys_addrof(request->closeDir));
         break;
      case FSACommand::CloseFile:
         status = device->closeFile(phys_addrof(request->closeFile));
         break;
      case FSACommand::FlushFile:
         status = device->flushFile(phys_addrof(request->flushFile));
         break;
      case FSACommand::FlushQuota:
         status = device->flushQuota(phys_addrof(request->flushQuota));
         break;
      case FSACommand::GetCwd:
         status = device->getCwd(phys_addrof(response->getCwd));
         break;
      case FSACommand::GetInfoByQuery:
         status = device->getInfoByQuery(phys_addrof(request->getInfoByQuery), phys_addrof(response->getInfoByQuery));
         break;
      case FSACommand::GetPosFile:
         status = device->getPosFile(phys_addrof(request->getPosFile), phys_addrof(response->getPosFile));
         break;
      case FSACommand::IsEof:
         status = device->isEof(phys_addrof(request->isEof));
         break;
      case FSACommand::MakeDir:
         status = device->makeDir(phys_addrof(request->makeDir));
         break;
      case FSACommand::OpenDir:
         status = device->openDir(phys_addrof(request->openDir), phys_addrof(response->openDir));
         break;
      case FSACommand::OpenFile:
         status = device->openFile(phys_addrof(request->openFile), phys_addrof(response->openFile));
         break;
      case FSACommand::ReadDir:
         status = device->readDir(phys_addrof(request->readDir), phys_addrof(response->readDir));
         break;
      case FSACommand::Remove:
         status = device->remove(phys_addrof(request->remove));
         break;
      case FSACommand::Rename:
         status = device->rename(phys_addrof(request->rename));
         break;
      case FSACommand::RewindDir:
         status = device->rewindDir(phys_addrof(request->rewindDir));
         break;
      case FSACommand::SetPosFile:
         status = device->setPosFile(phys_addrof(request->setPosFile));
         break;
      case FSACommand::StatFile:
         status = device->statFile(phys_addrof(request->statFile), phys_addrof(response->statFile));
         break;
      case FSACommand::TruncateFile:
         status = device->truncateFile(phys_addrof(request->truncateFile));
         break;
      default:
         status = FSAStatus::UnsupportedCmd;
      }
   }

   // TODO: Move this reply once we do asynchronous filesystem commands
   kernel::IOS_ResourceReply(resourceRequest, static_cast<Error>(status));
   return status;
}

static FSAStatus
fsaDeviceIoctlv(phys_ptr<kernel::ResourceRequest> resourceRequest,
                FSACommand command,
                be2_phys_ptr<IoctlVec> vecs)
{
   auto request = phys_ptr<FSARequest> { vecs[0].paddr };
   auto device = getDevice(resourceRequest->requestData.handle);
   auto status = FSAStatus::OK;

   if (!device) {
      status = FSAStatus::InvalidClientHandle;
   } else if (request->emulatedError < 0) {
      status = request->emulatedError;
   } else {
      switch (command) {
      case FSACommand::ReadFile:
      {
         auto buffer = phys_ptr<uint8_t> { vecs[1].paddr };
         auto length = vecs[1].len;
         status = device->readFile(phys_addrof(request->readFile), buffer, length);
         break;
      }
      case FSACommand::WriteFile:
      {
         auto buffer = phys_ptr<uint8_t> { vecs[1].paddr };
         auto length = vecs[1].len;
         status = device->writeFile(phys_addrof(request->writeFile), buffer, length);
         break;
      }
      case FSACommand::Mount:
      {
         status = device->mount(phys_addrof(request->mount));
         break;
      }
      default:
         status = FSAStatus::UnsupportedCmd;
      }
   }

   // TODO: Move this reply once we do asynchronous filesystem commands
   kernel::IOS_ResourceReply(resourceRequest, static_cast<Error>(status));
   return status;
}

static Error
fsaThreadMain(phys_ptr<void> /*context*/)
{
   StackObject<kernel::Message> message;
   StackObject<RequestOrigin> origin;

   while (true) {
      auto error = kernel::IOS_ReceiveMessage(sData->fsaMessageQueue,
                                              message,
                                              kernel::MessageFlags::NonBlocking);
      if (error < Error::OK) {
         return error;
      }

      auto request = phys_ptr<kernel::ResourceRequest>(phys_addr { static_cast<kernel::Message>(*message) });
      switch (request->requestData.command) {
      case Command::Open:
      {
         FSADeviceHandle fsaHandle;
         origin->titleId = request->requestData.titleId;
         origin->processId = request->requestData.processId;
         origin->groupId = request->requestData.groupId;

         auto error = fsaDeviceOpen(origin,
                                    &fsaHandle,
                                    request->requestData.args.open.caps);
         if (error >= FSAStatus::OK) {
            error = static_cast<FSAStatus>(fsaHandle);
         }

         kernel::IOS_ResourceReply(request, static_cast<Error>(error));
         break;
      }

      case Command::Close:
      {
         auto error = fsaDeviceClose(request->requestData.handle, request);
         kernel::IOS_ResourceReply(request, static_cast<Error>(error));
         break;
      }

      case Command::Ioctl:
      {
         fsaDeviceIoctl(request,
                        static_cast<FSACommand>(request->requestData.args.ioctl.request),
                        request->requestData.args.ioctl.inputBuffer,
                        request->requestData.args.ioctl.outputBuffer);
         break;
      }

      case Command::Ioctlv:
      {
         fsaDeviceIoctlv(request,
                         static_cast<FSACommand>(request->requestData.args.ioctlv.request),
                         request->requestData.args.ioctlv.vecs);
         break;
      }

      default:
         kernel::IOS_ResourceReply(request, Error::Invalid);
      }
   }
}

Error
startFsaThread()
{
   auto error = kernel::IOS_CreateMessageQueue(phys_addrof(sData->fsaMessageBuffer),
                                               static_cast<uint32_t>(sData->fsaMessageBuffer.size()));
   if (error < Error::OK) {
      return error;
   }

   auto queueId = static_cast<kernel::MessageQueueId>(error);
   sData->fsaMessageQueue = queueId;

   error = kernel::IOS_RegisterResourceManager("/dev/fsa",
                                               sData->fsaMessageQueue);
   if (error < Error::OK) {
      return error;
   }

   error = kernel::IOS_SetResourcePermissionGroup("/dev/fsa",
                                                  kernel::ResourcePermissionGroup::FS);
   if (error < Error::OK) {
      return error;
   }

   error = kernel::IOS_CreateThread(fsaThreadMain,
                                    nullptr,
                                    sData->fsaThreadStack.phys_data() + sData->fsaThreadStack.size(),
                                    static_cast<uint32_t>(sData->fsaThreadStack.size()),
                                    78,
                                    kernel::ThreadFlags::Detached);
   if (error < Error::OK) {
      return error;
   }

   auto threadId = static_cast<kernel::ThreadId>(error);
   sData->fsaThread = threadId;

   error = kernel::IOS_StartThread(sData->fsaThread);
   if (error < Error::OK) {
      return error;
   }

   return Error::OK;
}

} // namespace ios::fs::internal

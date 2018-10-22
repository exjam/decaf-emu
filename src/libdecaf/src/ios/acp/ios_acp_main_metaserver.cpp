#include "ios_acp_log.h"
#include "ios_acp_main_metaserver.h"
#include "ios_acp_nn_miscservice.h"
#include "ios_acp_nn_saveservice.h"

#include "ios/ios_handlemanager.h"
#include "ios/ios_stackobject.h"
#include "ios/kernel/ios_kernel_process.h"
#include "ios/kernel/ios_kernel_resourcemanager.h"
#include "ios/kernel/ios_kernel_thread.h"
#include "ios/mcp/ios_mcp_ipc.h"
#include "ios/nn/ios_nn_metaserver.h"

namespace ios::acp::internal
{

using namespace kernel;

constexpr auto AcpMainNumMessages = 100u;
constexpr auto AcpMainThreadStackSize = 0x10000u;
constexpr auto AcpMainThreadPriority = 50u;

class AcpMainMetaServer : public nn::MetaServer
{
public:
   AcpMainMetaServer() :
      nn::MetaServer(true)
   {
   }

   void intialiseServer() override
   {
      // Asociate resource manager
   }

   void finaliseServer() override
   {
   }
};

struct StaticAcpMainThreadData
{
   be2_struct<AcpMainMetaServer> metaServer;
   be2_array<Message, AcpMainNumMessages> messageBuffer;
   be2_array<uint8_t, AcpMainThreadStackSize> threadStack;
};

static phys_ptr<StaticAcpMainThreadData> sMainMetaServerData = nullptr;

Error
startMainMetaServer()
{
   auto &metaServer = sMainMetaServerData->metaServer;
   auto result = metaServer.initialise("/dev/acp_main",
                                       phys_addrof(sMainMetaServerData->messageBuffer),
                                       static_cast<uint32_t>(sMainMetaServerData->messageBuffer.size()));
   if (result.failed()) {
      internal::acpLog->error(
         "startMainMetaServer: MetaServer initialisation failed for /dev/acp_main, result = {}",
         result.code);
      return Error::FailInternal;
   }

   // TODO: Services 1, 3, 4, 6, 301, 302, 303, 304
   metaServer.registerService<MiscService>();
   metaServer.registerService<SaveService>();

   result = metaServer.start(phys_addrof(sMainMetaServerData->threadStack) + sMainMetaServerData->threadStack.size(),
                             static_cast<uint32_t>(sMainMetaServerData->threadStack.size()),
                             AcpMainThreadPriority);
   if (result.failed()) {
      internal::acpLog->error(
         "startMainMetaServer: MetaServer start failed for /dev/acp_main, result = {}",
         result.code);
      return Error::FailInternal;
   }

   return Error::OK;
}

void
initialiseStaticMainMetaServerData()
{
   sMainMetaServerData = allocProcessStatic<StaticAcpMainThreadData>();
}

} // namespace ios::acp::internal

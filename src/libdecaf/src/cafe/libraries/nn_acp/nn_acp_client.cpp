#pragma optimize("", off)
#include "nn_acp.h"
#include "nn_acp_client.h"

#include "cafe/cafe_stackobject.h"
#include "cafe/libraries/coreinit/coreinit_mutex.h"
#include "cafe/libraries/coreinit/coreinit_systeminfo.h"
#include "nn/acp/nn_acp_result.h"
#include "nn/ipc/nn_ipc_client.h"

using namespace cafe::coreinit;
using namespace nn::acp;

namespace cafe::nn_acp
{

struct StaticClientData
{
   StaticClientData()
   {
      OSInitMutex(virt_addrof(mutex));
   }

   alignas(64) be2_array<uint8_t, 0x3000> allocatorMemory;

   be2_struct<OSMutex> mutex;
   be2_val<uint32_t> refCount = 0u;
   be2_struct<nn::ipc::Client> client;
   be2_struct<nn::ipc::TransferableBufferAllocator> allocator;
};

static virt_ptr<StaticClientData> sClientData = nullptr;

nn::Result
ACPInitialize()
{
   OSLockMutex(virt_addrof(sClientData->mutex));
   if (sClientData->refCount == 0) {
      sClientData->client.Initialize(make_stack_string("/dev/acp_main"));
      sClientData->allocator.Initialize(virt_addrof(sClientData->allocatorMemory),
                                        sClientData->allocatorMemory.size());

      // TODO: MCP_Open
      // TODO: nn::spm::Initialize
      // TODO: nn::spm::StartFatalDetection

      auto upid = OSGetUPID();
      if (upid == kernel::UniqueProcessId::HomeMenu ||
          upid == kernel::UniqueProcessId::Game) {
         // TODO: PrepareToSetOwnApplicationTitleId
         // TODO: ACPSetOwnApplicationTitleId
      }
   }

   sClientData->refCount++;
   OSUnlockMutex(virt_addrof(sClientData->mutex));
   return ResultSuccess;
}

void
ACPFinalize()
{
   OSLockMutex(virt_addrof(sClientData->mutex));
   OSUnlockMutex(virt_addrof(sClientData->mutex));
}

namespace internal
{

virt_ptr<nn::ipc::Client>
getClient()
{
   return virt_addrof(sClientData->client);
}

virt_ptr<nn::ipc::TransferableBufferAllocator>
getAllocator()
{
   return virt_addrof(sClientData->allocator);
}

} // namespace internal

void
Library::registerClientSymbols()
{
   RegisterFunctionExport(ACPInitialize);
   RegisterFunctionExport(ACPFinalize);

   RegisterDataInternal(sClientData);
}

}  // namespace cafe::nn_acp

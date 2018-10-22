#pragma optimize("", off)
#include "nn_acp.h"
#include "nn_acp_client.h"
#include "nn_acp_miscservice.h"

#include "cafe/libraries/cafe_hle_stub.h"
#include "nn/acp/nn_acp_result.h"
#include "nn/acp/nn_acp_miscservice.h"

using namespace nn::acp;

namespace cafe::nn_acp
{

//! 01/01/2000 @ 12:00am
static constexpr auto NetworkTimeEpoch = 946684800000000;

/**
 * Sets outTime to microseconds since our NetworkTimeEpoch - 01/01/2000
 */
nn::Result
ACPGetNetworkTime(virt_ptr<uint64_t> outTime,
                  virt_ptr<uint32_t> outUnknown)
{
   auto command = nn::ipc::ClientIpcData<services::MiscService::GetNetworkTime> { internal::getAllocator() };
   auto result = internal::getClient()->SendSyncRequest(command);
   if (result.failed()) {
      return result;
   }

   auto time = uint64_t { 0 };
   auto unk = uint32_t { 0 };
   result = command.ReadResponse(time, unk);
   if (result.ok()) {
      *outTime = time - NetworkTimeEpoch;
      *outUnknown = unk;
   }

   return result;
}

nn::Result
ACPGetTitleMetaXml(uint64_t titleId,
                   virt_ptr<services::ACPMetaXml> data)
{
   auto command = nn::ipc::ClientIpcData<services::MiscService::GetTitleMetaXml> { internal::getAllocator() };
   command.SetParameters(data, titleId);
   auto result = internal::getClient()->SendSyncRequest(command);
   if (result.failed()) {
      return result;
   }

   return command.ReadResponse();
}

void
Library::registerTitleSymbols()
{
   RegisterFunctionExport(ACPGetTitleMetaXml);
   RegisterFunctionExport(ACPGetNetworkTime);
   RegisterFunctionExportName("GetTitleMetaXml__Q2_2nn3acpFULP11_ACPMetaXml",
                              ACPGetTitleMetaXml);
}

}  // namespace cafe::nn_acp

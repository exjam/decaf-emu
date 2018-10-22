#pragma once
#include "nn/ipc/nn_ipc_command.h"
#include "nn/ipc/nn_ipc_managedbuffer.h"
#include "nn/ipc/nn_ipc_service.h"

#include <cstdint>
#include <common/structsize.h>

namespace nn::acp::services
{

struct ACPMetaXml
{
   UNKNOWN(0x3440);
};
CHECK_SIZE(ACPMetaXml, 0x3440);

struct MiscService : ipc::Service<0>
{
   using GetNetworkTime =
      ipc::Command<MiscService, 101>
         ::Parameters<>
         ::Response<uint64_t, uint32_t>;

   using GetTitleMetaXml =
      ipc::Command<MiscService, 205>
         ::Parameters<ipc::OutBuffer<ACPMetaXml>, uint64_t>;
};

} // namespace nn::acp::services

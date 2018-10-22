#pragma once
#include "ios/nn/ios_nn_metaserver.h"
#include "nn/acp/nn_acp_miscservice.h"

namespace ios::acp::internal
{

struct MiscService : ::nn::acp::services::MiscService
{
   static nn::Result
   commandHandler(uint32_t unk1,
                  nn::CommandId command,
                  nn::CommandHandlerArgs &args);
};

} // namespace ios::acp

#include "ios_acp_nn_saveservice.h"

#include "nn/acp/nn_acp_result.h"

using namespace nn::acp;

namespace ios::acp::internal
{

nn::Result
SaveService::commandHandler(uint32_t unk1,
                            nn::CommandId command,
                            nn::CommandHandlerArgs &args)
{
   return ResultSuccess;
}

} // namespace ios::acp::internal

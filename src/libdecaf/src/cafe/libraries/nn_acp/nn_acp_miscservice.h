#pragma once
#include "nn/nn_result.h"
#include "nn/acp/nn_acp_miscservice.h"

#include <libcpu/be2_struct.h>

namespace cafe::nn_acp
{

nn::Result
ACPGetTitleMetaXml(uint64_t titleId,
                   virt_ptr<nn::acp::services::ACPMetaXml> data);

}  // namespace cafe::nn_acp

#pragma once
#include "nn/nn_result.h"
#include "nn/ipc/nn_ipc_client.h"
#include "nn/ipc/nn_ipc_transferablebufferallocator.h"

namespace cafe::nn_acp
{

nn::Result
ACPInitialize();

void
ACPFinalize();

namespace internal
{

virt_ptr<nn::ipc::Client>
getClient();

virt_ptr<nn::ipc::TransferableBufferAllocator>
getAllocator();

} // namespace internal

} // namespace cafe::nn_acp

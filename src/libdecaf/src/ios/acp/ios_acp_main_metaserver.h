#pragma once
#include "ios/ios_enum.h"
#include "ios/kernel/ios_kernel_messagequeue.h"

namespace ios::acp::internal
{

Error
startMainMetaServer();

void
initialiseStaticMainMetaServerData();

} // namespace ios::acp::internal

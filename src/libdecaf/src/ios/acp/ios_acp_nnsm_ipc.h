#pragma once
#include "ios/ios_enum.h"
#include <string_view>

namespace ios::acp
{

Error
NNSM_RegisterMetaServer(std::string_view device);

Error
NNSM_UnregisterMetaServer(std::string_view device);

} // namespace ios::acp

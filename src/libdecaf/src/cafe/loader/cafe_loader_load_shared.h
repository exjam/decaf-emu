#pragma once
#include "cafe_loader_load_init.h"

namespace cafe::loader::internal
{

int32_t
initialiseSharedHeaps();

virt_ptr<LOADED_RPL>
findLoadedSharedModule(std::string_view moduleName);

int32_t
LiInitSharedForProcess(virt_ptr<RPL_STARTINFO> initData);

} // namespace cafe::loader::internal

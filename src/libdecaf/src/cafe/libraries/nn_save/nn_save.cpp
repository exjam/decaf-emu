#include "nn_save.h"
#include "cafe/libraries/coreinit/coreinit_dynload.h"

namespace cafe::nn::save
{

static int32_t
rpl_entry(coreinit::OSDynLoad_ModuleHandle moduleHandle,
          coreinit::OSDynLoad_EntryReason reason)
{
   return 0;
}

void
Library::registerSymbols()
{
   RegisterEntryPoint(rpl_entry);

   registerCmdSymbols();
   registerPathSymbols();
}

} // namespace cafe::nn::save

#include "nn_act.h"
#include "cafe/libraries/coreinit/coreinit_dynload.h"

namespace cafe::nn::act
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

   registerStubSymbols();
}

} // namespace cafe::nn::act

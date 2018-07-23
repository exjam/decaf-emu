#include "sndcore2.h"
#include "cafe/libraries/coreinit/coreinit_dynload.h"

namespace cafe::sndcore2
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
}

} // namespace cafe::sndcore2

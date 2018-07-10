#include "tcl.h"
#include "cafe/libraries/coreinit/coreinit_dynload.h"

namespace cafe::tcl
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

   registerDriverSymbols();
   registerRegisterSymbols();
   registerRingSymbols();
}

} // namespace cafe::tcl

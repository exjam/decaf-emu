#include "nn_olv.h"
#include "cafe/libraries/coreinit/coreinit_dynload.h"

namespace cafe::nn::olv
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

   registerDownloadedCommunityDataSymbols();
   registerDownloadedTopicDataSymbols();
   registerInitSymbols();
   registerUploadedDataBaseSymbols();
   registerUploadedPostDataSymbols();
}

} // namespace cafe::nn::olv

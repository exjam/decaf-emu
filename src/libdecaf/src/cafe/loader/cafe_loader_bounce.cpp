#include "cafe_loader_iop.h"

#include <common/cbool.h>

using namespace ios::mcp;
using namespace cafe::kernel;

namespace cafe::loader::internal
{

struct StaticBounceData
{
   be2_val<BOOL> dynloadInitialised;
};

static virt_ptr<StaticBounceData>
sBounceData;

void
LiWaitOneChunk(uint32_t *outBytesRead,
               std::string_view name,
               MCPLibraryType libraryType)
{
   uint32_t bytesRead;

   if (sBounceData->dynloadInitialised) {
      LiWaitIopCompleteWithInterrupts(&bytesRead);
   } else {
      LiWaitIopComplete(&bytesRead);
   }

}

void
LiBounceOneChunk(std::string_view name,
                 MCPLibraryType libraryType,
                 UniqueProcessId upid,
                 int * hunkBytes,
                 uint32_t pos,
                 int bufferNumber,
                 int * dst_address)
{
   auto rampid = getRampidFromUpid(upid);
   auto outBuffer = virt_cast<void *>(virt_addr { 0xF6000000 });
   if (libraryType == MCPLibraryType::CafeOS) {
      outBuffer = virt_cast<void *>(virt_addr { 0xF6400000 });
   }

   LiLoadAsync(name, outBuffer, 0x400000, pos, libraryType, rampid);
}

} // namespace cafe::loader::internal

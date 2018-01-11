#include "cafe_loader_iop.h"

#include <common/bitutils.h>
#include <common/cbool.h>

using namespace ios::mcp;
using namespace cafe::kernel;

namespace cafe::loader::internal
{

struct StaticBounceData
{
   be2_val<BOOL> dynloadInitialised;
   be2_val<uint32_t> sgFinishedLoadingBuffer; // 0xEFE19E80
   be2_val<MCPLibraryType> sgFileType; // 0xEFE19E84
   be2_val<UniqueProcessId> sgProcId; // 0xEFE19E88
   be2_val<uint32_t> sgGotBytes; // 0xEFE19E8C
   be2_val<uint32_t> sgTotalBytes; // 0xEFE19E90
   be2_val<uint32_t> sgFileOffset; // 0xEFE19E94
   be2_val<uint32_t> sgBufferNumber; // 0xEFE19E98
   be2_val<ios::Error> sgBounceError; // 0xEFE19E9C
   be2_array<char, 0x1000> sgLoadName; // 0xEFE19EA0
};

static virt_ptr<StaticBounceData>
sBounceData;

void
LiInitBuffer(bool unk)
{
   if (unk) {
      sBounceData->sgFinishedLoadingBuffer = 1u;
   }

   sBounceData->sgFileType = MCPLibraryType::Executable;
   sBounceData->sgFileOffset = 0u;
   sBounceData->sgBufferNumber = 0u;
   sBounceData->sgProcId = UniqueProcessId::Invalid;
   sBounceData->sgLoadName[0] = char { 0 };
   sBounceData->sgTotalBytes = 0u;
   sBounceData->sgBounceError = ios::Error::OK;
   sBounceData->sgGotBytes = 0u;
}

constexpr auto ChunkSize = 0x400000u;
constexpr auto ChunkBuffer = virt_addr { 0xF6000000u };
constexpr auto ChunkBuffer1 = virt_addr { 0xF6400000u };

static inline uint32_t
cntlzw(uint32_t s)
{
   unsigned long a;

   if (!bit_scan_reverse(&a, s)) {
      a = 32;
   } else {
      a = 31 - a;
   }

   return a;
}

int32_t
LiBounceOneChunk(std::string_view name,
                 MCPLibraryType fileType,
                 UniqueProcessId upid,
                 virt_ptr<uint32_t> outHunkBytes,
                 uint32_t offset,
                 uint32_t bufferNumber,
                 virt_ptr<virt_ptr<void>> outDstPtr)
{
   LiCheckAndHandleInterrupts();

   auto dst = virt_cast<void *>(ChunkBuffer);
   if (bufferNumber == 1) {
      dst = virt_cast<void *>(ChunkBuffer1);
   }

   sBounceData->sgLoadName = name;
   sBounceData->sgProcId = upid;
   sBounceData->sgFileOffset = offset;
   sBounceData->sgBufferNumber = bufferNumber;
   sBounceData->sgFileType = fileType;

   auto error = LiLoadAsync(name, dst, ChunkSize, offset, fileType, getRampidFromUpid(upid));
   sBounceData->sgBounceError = error;

   if (error < ios::Error::OK) {
      // LiSetFatalError
      LiCheckAndHandleInterrupts();
      return error;
   }

   if (outHunkBytes) {
      *outHunkBytes = ChunkSize;
   }

   if (outDstPtr) {
      *outDstPtr = dst;
   }

   // ????
   sBounceData->sgFinishedLoadingBuffer = cntlzw(sBounceData->sgFinishedLoadingBuffer) >> 5;
   LiCheckAndHandleInterrupts();
   return 0;
}

int32_t
LiWaitOneChunk(uint32_t *outBytesRead,
               std::string_view name,
               MCPLibraryType fileType)
{
   uint32_t bytesRead;
   auto iosError = ios::Error::OK;

   if (sBounceData->dynloadInitialised) {
      iosError = LiWaitIopCompleteWithInterrupts(&bytesRead);
   } else {
      iosError = LiWaitIopComplete(&bytesRead);
   }

   if (iosError < ios::Error::OK) {
      return static_cast<int32_t>(iosError);
   }

   sBounceData->sgBounceError = iosError;
   sBounceData->sgGotBytes = bytesRead;

   // !???!??
   sBounceData->sgFinishedLoadingBuffer = cntlzw(sBounceData->sgFinishedLoadingBuffer) >> 5;

   if (outBytesRead) {
      *outBytesRead = bytesRead;
   }

   sBounceData->sgTotalBytes += bytesRead;
   return 0;
}

} // namespace cafe::loader::internal

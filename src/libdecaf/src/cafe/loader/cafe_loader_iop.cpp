#include "cafe_loader_iop.h"
#include "cafe_loader_ipcldriver.h"
//#include "cafe_loader_process.h"

#include "cafe/kernel/cafe_kernel_ipckdriver.h"
//#include "cafe/kernel/cafe_kernel_mcp.h"
#include "cafe/kernel/cafe_kernel_process.h"
#include "ios/mcp/ios_mcp_mcp.h"

#include <array>
#include <cstdint>
#include <common/cbool.h>
#include <common/teenyheap.h>
#include <libcpu/be2_struct.h>
#include <libcpu/cpu.h>

using namespace ios::mcp;
using namespace cafe::kernel;

namespace cafe::loader::internal
{

constexpr auto HeapSizePerCore = 0x2000u;
constexpr auto HeapAllocAlign = 0x40u;

struct LiLoadReply
{
   be2_val<BOOL> done;
   be2_virt_ptr<void> requestBuffer;
   be2_val<ios::Error> error;
   be2_val<BOOL> pending;
};

struct StaticIopData
{
   be2_val<uint32_t> mcpHandle;
   be2_struct<LiLoadReply> loadReply;
};

static virt_ptr<StaticIopData>
sIopData;

static std::array<virt_ptr<void>, 3>
sHeapBufs;

static std::array<TeenyHeap, 3>
sHeaps;

static virt_ptr<void>
iop_percore_getheap()
{
   return sHeapBufs.at(cpu::this_core::id());
}

static void
iop_percore_initheap()
{
   auto id = cpu::this_core::id();
   auto heap = sHeapBufs.at(cpu::this_core::id());
   sHeaps.at(id).reset(heap.getRawPointer(), HeapSizePerCore);
}

static virt_ptr<void>
iop_percore_malloc(uint32_t size)
{
   size = align_up(size, HeapAllocAlign);
   auto id = cpu::this_core::id();
   auto ptr = sHeaps[id].alloc(size, HeapAllocAlign);
   return virt_cast<void *>(cpu::translate(ptr));
}

static void
iop_percore_free(virt_ptr<void> buffer)
{
   auto id = cpu::this_core::id();
   sHeaps[id].free(buffer.getRawPointer());
}

void
LiInitIopInterface()
{
   iop_percore_initheap();

   if (sIopData->mcpHandle <= 0) {
      sIopData->mcpHandle = loaderGetMcpHandle();
   }
}

static void
Loader_AsyncCallback(ios::Error error,
                     virt_ptr<void> context)
{
   auto reply = virt_cast<LiLoadReply *>(context);
   if (!reply) {
      return;
   }

   if (reply->requestBuffer) {
      iop_percore_free(reply->requestBuffer);
      reply->requestBuffer = nullptr;
   }

   reply->error = error;
   reply->done = TRUE;
}

ios::Error
LiLoadAsync(std::string_view name,
            virt_ptr<void> outBuffer,
            uint32_t outBufferSize,
            uint32_t pos,
            MCPFileType fileType,
            RamProcessId rampid)
{
   auto request = virt_cast<MCPRequestLoadFile *>(iop_percore_malloc(sizeof(MCPRequestLoadFile)));
   request->pos = pos;
   request->fileType = fileType;
   request->cafeProcessId = static_cast<uint32_t>(rampid);
   request->name = name;

   sIopData->loadReply.done = FALSE;
   sIopData->loadReply.pending = TRUE;
   sIopData->loadReply.requestBuffer = request;
   sIopData->loadReply.error = ios::Error::InvalidArg;

   auto error = IPCLDriver_IoctlAsync(sIopData->mcpHandle,
                                      MCPCommand::LoadFile,
                                      request, sizeof(MCPRequestLoadFile),
                                      outBuffer, outBufferSize,
                                      &Loader_AsyncCallback,
                                      virt_addrof(sIopData->loadReply));
   if (error < ios::Error::OK) {
      iop_percore_free(request);
   }

   return error;
}

static ios::Error
LiPollForCompletion()
{
   virt_ptr<IPCLDriver> driver;
   auto error = IPCLDriver_GetInstance(&driver);
   if (error < ios::Error::OK) {
      return error;
   }

   auto request = loaderPollIpckCompletion();
   if (!request) {
      return ios::Error::QEmpty;
   }

   return IPCLDriver_ProcessReply(driver, request);
}

void
LiCheckAndHandleInterrupts()
{
}

int32_t
LiCheckInterrupts()
{
   return 0;
}

static ios::Error
LiWaitAsyncReply(virt_ptr<LiLoadReply> reply)
{
   while (!reply->done) {
      LiPollForCompletion();
   }

   reply->done = FALSE;
   reply->pending = FALSE;
   return reply->error;
}

static ios::Error
LiWaitAsyncReplyWithInterrupts(virt_ptr<LiLoadReply> reply)
{
   while (!reply->done) {
      LiPollForCompletion();

      auto interrupts = LiCheckInterrupts();
      if (interrupts) {
         // Handle the interrupt
         // Loader_SysCallRPLLoaderResumeContext
      }
   }

   reply->done = FALSE;
   reply->pending = FALSE;
   return reply->error;
}

ios::Error
LiWaitIopComplete(uint32_t *outBytesRead)
{
   auto error = LiWaitAsyncReply(virt_addrof(sIopData->loadReply));
   if (error < 0) {
      *outBytesRead = 0;
   } else {
      *outBytesRead = static_cast<uint32_t>(error);
      error = ios::Error::OK;
   }

   return error;
}

ios::Error
LiWaitIopCompleteWithInterrupts(uint32_t *outBytesRead)
{
   auto error = LiWaitAsyncReplyWithInterrupts(virt_addrof(sIopData->loadReply));
   if (error < 0) {
      *outBytesRead = 0;
   } else {
      *outBytesRead = static_cast<uint32_t>(error);
      error = ios::Error::OK;
   }

   return error;
}

void
initialiseStaticIopData()
{
   sIopData = virt_cast<StaticIopData *>(allocProcessStatic(sizeof(StaticIopData)));
   sHeapBufs[0] = allocProcessStatic(HeapSizePerCore, HeapAllocAlign);
   sHeapBufs[1] = allocProcessStatic(HeapSizePerCore, HeapAllocAlign);
   sHeapBufs[2] = allocProcessStatic(HeapSizePerCore, HeapAllocAlign);
}

} // namespace cafe::loader::internal

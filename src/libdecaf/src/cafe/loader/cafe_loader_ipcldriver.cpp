#include "cafe_loader_ipcldriver.h"
#include "cafe/kernel/cafe_kernel_ipckdriver.h"
#include "cafe/cafe_stackobject.h"

#include <libcpu/cpu.h>

using namespace cafe::kernel;

namespace cafe::loader::internal
{

struct StaticIpclData
{
   be2_array<IPCLDriver, 3> drivers;
   be2_array<IPCKDriverRequest, IPCLBufferCount * 3> ipclResourceRequestBuffer;
};

static virt_ptr<StaticIpclData>
sIpclData;

ios::Error
IPCLDriver_Init()
{
   virt_ptr<IPCLDriver> driver;
   IPCLDriver_GetInstance(&driver);

   std::memset(driver.getRawPointer(), 0, sizeof(IPCLDriver));
   driver->coreId = cpu::this_core::id();
   driver->ipckRequestBuffer = virt_addrof(sIpclData->ipclResourceRequestBuffer[driver->coreId]);
   driver->status = IPCLDriverStatus::Initialised;

   std::memset(driver->ipckRequestBuffer.getRawPointer(), 0, sizeof(IPCKDriverRequest) * IPCLBufferCount);

   return ios::Error::OK;
}

ios::Error
IPCLDriver_InitRequestParameterBlocks(virt_ptr<IPCLDriver> driver)
{
   for (auto i = 0u; i < IPCLBufferCount; ++i) {
      auto ipckRequestBuffer = virt_addrof(driver->ipckRequestBuffer[i]);
      auto &request = driver->requests[i];
      request.ipckRequestBuffer = ipckRequestBuffer;
      request.asyncCallback = nullptr;
      request.asyncContext = nullptr;
   }

   return ios::Error::OK;
}

void
IPCLDriver_FIFOInit(virt_ptr<IPCLDriverFIFO> fifo)
{
   fifo->pushIndex = 0;
   fifo->popIndex = -1;
   fifo->count = 0;
   fifo->requests.fill(nullptr);
}

/**
 * Push a request into an IPCLDriverFIFO structure
 *
 * \retval ios::Error::OK
 * Success.
 *
 * \retval ios::Error::QFull
 * There was no free space in the queue to push the request.
 */
ios::Error
IPCLDriver_FIFOPush(virt_ptr<IPCLDriverFIFO> fifo,
                    virt_ptr<IPCLDriverRequest> request)
{
   if (fifo->pushIndex == fifo->popIndex) {
      return ios::Error::QFull;
   }

   fifo->requests[fifo->pushIndex] = request;

   if (fifo->popIndex == -1) {
      fifo->popIndex = fifo->pushIndex;
   }

   fifo->count += 1;
   fifo->pushIndex = static_cast<int32_t>((fifo->pushIndex + 1) % IPCLBufferCount);

   if (fifo->count > fifo->maxCount) {
      fifo->maxCount = fifo->count;
   }

   return ios::Error::OK;
}

/**
 * Pop a request into an IPCLDriverFIFO structure.
 *
 * \retval ios::Error::OK
 * Success.
 *
 * \retval ios::Error::QEmpty
 * There was no requests to pop from the queue.
 */
ios::Error
IPCLDriver_FIFOPop(virt_ptr<IPCLDriverFIFO> fifo,
                   virt_ptr<IPCLDriverRequest> *outRequest)
{
   if (fifo->popIndex == -1) {
      return ios::Error::QEmpty;
   }

   auto request = fifo->requests[fifo->popIndex];
   fifo->count -= 1;

   if (fifo->count == 0) {
      fifo->popIndex = -1;
   } else {
      fifo->popIndex = static_cast<int32_t>((fifo->popIndex + 1) % IPCLBufferCount);
   }

   *outRequest = request;
   return ios::Error::OK;
}

/**
 * Peek the next request which would be popped from a IPCLDriverFIFO structure.
 *
 * \retval ios::Error::OK
 * Success.
 *
 * \retval ios::Error::QEmpty
 * There was no requests to pop from the queue.
 */
ios::Error
IPCLDriver_PeekFIFO(virt_ptr<IPCLDriverFIFO> fifo,
                    virt_ptr<virt_ptr<IPCLDriverRequest>> outRequest)
{
   if (fifo->popIndex == -1) {
      return ios::Error::QEmpty;
   }

   *outRequest = fifo->requests[fifo->popIndex];
   return ios::Error::OK;
}

ios::Error
IPCLDriver_Open()
{
   virt_ptr<IPCLDriver> driver;
   IPCLDriver_GetInstance(&driver);

   if (driver->status != IPCLDriverStatus::Closed && driver->status != IPCLDriverStatus::Initialised) {
      return ios::Error::NotReady;
   }

   IPCLDriver_InitRequestParameterBlocks(driver);

   IPCLDriver_FIFOInit(virt_addrof(driver->freeFifo));
   IPCLDriver_FIFOInit(virt_addrof(driver->outboundFifo));

   for (auto i = 0u; i < IPCLBufferCount; ++i) {
      IPCLDriver_FIFOPush(virt_addrof(driver->freeFifo),
                          virt_addrof(driver->requests[i]));
   }

   cafe::kernel::IPCKDriver_Open(0, nullptr, nullptr, nullptr, nullptr);
   driver->status = IPCLDriverStatus::Open;
   return ios::Error::OK;
}

ios::Error
IPCLDriver_GetInstance(virt_ptr<IPCLDriver> *outDriver)
{
   auto driver = virt_addrof(sIpclData->drivers[cpu::this_core::id()]);
   *outDriver = driver;

   if (driver->status < IPCLDriverStatus::Open) {
      return ios::Error::NotReady;
   } else {
      return ios::Error::OK;
   }
}

ios::Error
IPCLDriver_AllocateRequestBlock(virt_ptr<IPCLDriver> driver,
                                virt_ptr<IPCLDriverRequest> *outRequest,
                                int32_t handle,
                                ios::Command command,
                                IPCLAsyncCallbackFn callback,
                                virt_ptr<void> callbackContext)
{
   virt_ptr<IPCLDriverRequest> request;
   auto error = IPCLDriver_FIFOPop(virt_addrof(driver->freeFifo), &request);
   if (error < ios::Error::OK) {
      driver->failedAllocateRequestBlock++;
      return error;
   }

   // Initialise IPCLDriverRequest
   request->allocated = TRUE;
   request->asyncCallback = callback;
   request->asyncContext = callbackContext;

   // Initialise IPCKDriverRequest
   auto ipckRequest = request->ipckRequestBuffer;
   std::memset(virt_addrof(ipckRequest->request.args).getRawPointer(), 0, sizeof(ipckRequest->request.args));
   ipckRequest->request.command = command;
   ipckRequest->request.handle = handle;
   ipckRequest->request.flags = 0u;
   ipckRequest->request.clientPid = 0;
   ipckRequest->request.reply = ios::Error::OK;

   *outRequest = request;
   return ios::Error::OK;
}

ios::Error
IPCLDriver_FreeRequestBlock(virt_ptr<IPCLDriver> driver,
                            virt_ptr<IPCLDriverRequest> request)
{
   auto error = IPCLDriver_FIFOPush(virt_addrof(driver->freeFifo), request);
   request->allocated = FALSE;

   if (error < ios::Error::OK) {
      driver->failedFreeRequestBlock++;
   }

   return error;
}

static ios::Error
defensiveProcessIncomingMessagePointer(virt_ptr<IPCLDriver> driver,
                                       virt_ptr<cafe::kernel::IPCKDriverRequest> ipckRequest,
                                       virt_ptr<IPCLDriverRequest> *outIpclRequest)
{
   if (ipckRequest < driver->ipckRequestBuffer) {
      return ios::Error::Invalid;
   }

   auto index = ipckRequest - driver->ipckRequestBuffer;
   if (index >= IPCLBufferCount) {
      return ios::Error::Invalid;
   }

   if (!driver->requests[index].allocated) {
      driver->invalidReplyMessagePointerNotAlloc++;
      return ios::Error::Invalid;
   }

   *outIpclRequest = virt_addrof(driver->requests[index]);
   return ios::Error::OK;
}

static void
ipclProcessReply(virt_ptr<IPCLDriver> driver,
                 virt_ptr<IPCLDriverRequest> request)
{
   driver->repliesReceived++;

   auto ipckRequest = request->ipckRequestBuffer;
   switch (ipckRequest->request.command) {
   case ios::Command::Open:
      if (request->asyncCallback) {
         if (ipckRequest->request.reply < ios::Error::OK) {
            driver->iosOpenAsyncRequestFail++;
         } else {
            driver->iosOpenAsyncRequestSuccess++;
         }
      }
      break;
   case ios::Command::Close:
      if (request->asyncCallback) {
         if (ipckRequest->request.reply < ios::Error::OK) {
            driver->iosCloseAsyncRequestFail++;
         } else {
            driver->iosCloseAsyncRequestSuccess++;
         }
      }
      break;
   case ios::Command::Read:
      decaf_check(ipckRequest->buffer1 || !ipckRequest->request.args.read.length);

      if (request->asyncCallback) {
         if (ipckRequest->request.reply < ios::Error::OK) {
            driver->iosReadAsyncRequestFail++;
         } else {
            driver->iosReadAsyncRequestSuccess++;
         }
      }
      break;
   case ios::Command::Write:
      decaf_check(ipckRequest->buffer1 || !ipckRequest->request.args.write.length);

      if (request->asyncCallback) {
         if (ipckRequest->request.reply < ios::Error::OK) {
            driver->iosWriteAsyncRequestFail++;
         } else {
            driver->iosWriteAsyncRequestSuccess++;
         }
      }
      break;
   case ios::Command::Seek:
      if (request->asyncCallback) {
         if (ipckRequest->request.reply < ios::Error::OK) {
            driver->iosSeekAsyncRequestFail++;
         } else {
            driver->iosSeekAsyncRequestSuccess++;
         }
      }
      break;
   case ios::Command::Ioctl:
      decaf_check(ipckRequest->buffer1 || !ipckRequest->request.args.ioctl.inputLength);
      decaf_check(ipckRequest->buffer2 || !ipckRequest->request.args.ioctl.outputLength);

      if (request->asyncCallback) {
         if (ipckRequest->request.reply < ios::Error::OK) {
            driver->iosIoctlAsyncRequestFail++;
         } else {
            driver->iosIoctlAsyncRequestSuccess++;
         }
      }
      break;
   case ios::Command::Ioctlv:
      decaf_check(ipckRequest->buffer1 ||
                  (ipckRequest->request.args.ioctlv.numVecIn + ipckRequest->request.args.ioctlv.numVecOut) == 0);

      if (request->asyncCallback) {
         if (ipckRequest->request.reply < ios::Error::OK) {
            driver->iosIoctlvAsyncRequestFail++;
         } else {
            driver->iosIoctlvAsyncRequestSuccess++;
         }
      }
      break;
   default:
      driver->invalidReplyCommand++;
   }
}

ios::Error
IPCLDriver_ProcessReply(virt_ptr<IPCLDriver> driver,
                        virt_ptr<cafe::kernel::IPCKDriverRequest> ipckRequest)
{
   virt_ptr<IPCLDriverRequest> request;

   if (driver->status < IPCLDriverStatus::Open) {
      driver->unexpectedReplyInterrupt++;
      return ios::Error::Invalid;
   }

   auto error = defensiveProcessIncomingMessagePointer(driver, ipckRequest, &request);
   if (error < ios::Error::OK) {
      driver->invalidReplyMessagePointer++;
      return error;
   }

   ipclProcessReply(driver, request);

   if (request->asyncCallback) {
      request->asyncCallback(ipckRequest->request.reply, request->asyncContext);
      IPCLDriver_FreeRequestBlock(driver, request);
      driver->asyncTransactionsCompleted++;
   }

   return error;
}

ios::Error
IPCLDriver_ProcessIOSIoctlRequest(virt_ptr<IPCLDriver> driver,
                                  virt_ptr<IPCLDriverRequest> request,
                                  uint32_t command,
                                  virt_ptr<const void> inputBuffer,
                                  uint32_t inputLength,
                                  virt_ptr<void> outputBuffer,
                                  uint32_t outputLength)
{
   auto ipckRequest = request->ipckRequestBuffer;
   ipckRequest->request.args.ioctl.request = command;
   ipckRequest->request.args.ioctl.inputBuffer = nullptr;
   ipckRequest->request.args.ioctl.inputLength = inputLength;
   ipckRequest->request.args.ioctl.outputBuffer = nullptr;
   ipckRequest->request.args.ioctl.outputLength = outputLength;

   ipckRequest->buffer1 = inputBuffer;
   ipckRequest->buffer2 = outputBuffer;
   return ios::Error::OK;
}

static ios::Error
sendFIFOToKernel(virt_ptr<IPCLDriver> driver)
{
   auto error = ios::Error::OK;
   virt_ptr<IPCLDriverRequest> poppedRequest;

   while (driver->status == IPCLDriverStatus::Open) {
      error = IPCLDriver_PeekFIFO(virt_addrof(driver->outboundFifo),
                                  virt_addrof(driver->currentSendTransaction));
      if (error < ios::Error::OK) {
         break;
      }

      driver->status = IPCLDriverStatus::Submitting;
      error = IPCKDriver_SubmitUserRequest(driver->currentSendTransaction->ipckRequestBuffer);
      IPCLDriver_FIFOPop(virt_addrof(driver->outboundFifo), &poppedRequest);
      decaf_check(poppedRequest == driver->currentSendTransaction);
      driver->status = IPCLDriverStatus::Open;

      if (error < ios::Error::OK) {
         break;
      }
   }

   return error;
}

ios::Error
IPCLDriver_SubmitRequestBlock(virt_ptr<IPCLDriver> driver,
                              virt_ptr<IPCLDriverRequest> request)
{
   // Flush out any pending requests
   sendFIFOToKernel(driver);

   auto error = IPCLDriver_FIFOPush(virt_addrof(driver->outboundFifo), request);
   if (error < ios::Error::OK) {
      driver->failedRequestSubmitOutboundFIFOFull++;
   } else {
      driver->requestsSubmitted++;
      error = sendFIFOToKernel(driver);
   }

   return error;
}

ios::Error
IPCLDriver_IoctlAsync(int32_t handle,
                      uint32_t command,
                      virt_ptr<const void> inputBuffer,
                      uint32_t inputLength,
                      virt_ptr<void> outputBuffer,
                      uint32_t outputLength,
                      IPCLAsyncCallbackFn callback,
                      virt_ptr<void> callbackContext)
{
   if (!inputBuffer && inputLength > 0) {
      return ios::Error::InvalidArg;
   }

   if (!outputBuffer && outputLength > 0) {
      return ios::Error::InvalidArg;
   }

   virt_ptr<IPCLDriver> driver;
   auto error = IPCLDriver_GetInstance(&driver);
   if (error < ios::Error::OK) {
      return error;
   }

   virt_ptr<IPCLDriverRequest> request;
   error = IPCLDriver_AllocateRequestBlock(driver,
                                           &request,
                                           handle,
                                           ios::Command::Ioctl,
                                           callback,
                                           callbackContext);
   if (error < ios::Error::OK) {
      return error;
   }

   error = IPCLDriver_ProcessIOSIoctlRequest(driver,
                                             request,
                                             command,
                                             inputBuffer,
                                             inputLength,
                                             outputBuffer,
                                             outputLength);

   if (error >= ios::Error::OK) {
      error = IPCLDriver_SubmitRequestBlock(driver, request);
   }

   if (error < ios::Error::OK) {
      IPCLDriver_FreeRequestBlock(driver, request);
      driver->iosIoctlAsyncRequestSubmitFail++;
   } else {
      driver->iosIoctlAsyncRequestSubmitSuccess++;
   }

   return error;
}

} // namespace cafe::loader::internal

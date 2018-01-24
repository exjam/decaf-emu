#include "coreinit.h"
#include "coreinit_core.h"
#include "coreinit_ipcdriver.h"
#include "coreinit_messagequeue.h"
#include "coreinit_thread.h"
#include "cafe/cafe_stackobject.h"

namespace cafe::coreinit
{

struct StaticIpcDriverData
{
   struct PerCoreData
   {
      be2_struct<IPCDriver> driver;
      be2_struct<OSThread> thread;
      be2_struct<OSMessageQueue> queue;
      be2_array<OSMessage, 0x30> messages;
      be2_array<uint8_t, 0x4000> stack;
      be2_array<IPCKDriverRequest, IPCBufferCount> ipcBuffers;
   };

   be2_array<char, 12> threadName0 = "IPC Core 0";
   be2_array<char, 12> threadName1 = "IPC Core 1";
   be2_array<char, 12> threadName2 = "IPC Core 2";
   be2_array<PerCoreData, CoreCount> perCoreData;
   be2_val<OSThreadEntryPointFn> threadEntryPoint;
};

static virt_ptr<StaticIpcDriverData>
getIpcDriverData()
{
   return Library::getStaticData()->ipcDriverData;
}

static virt_ptr<StaticIpcDriverData::PerCoreData>
getIpcDriverPerCoreData()
{
   return virt_addrof(Library::getStaticData()->ipcDriverData->perCoreData[OSGetCoreId()]);
}

/**
 * Initialise the IPC driver.
 */
void
IPCDriverInit()
{
   auto ipcDriverData = getIpcDriverData();
   auto perCoreData = getIpcDriverPerCoreData();

   auto driver = virt_addrof(perCoreData->driver);
   OSInitEvent(virt_addrof(driver->waitFreeFifoEvent), FALSE, OSEventMode::AutoReset);
   driver->status = IPCDriverStatus::Initialised;
   driver->coreId = OSGetCoreId();
   driver->ipcBuffers = virt_addrof(perCoreData->ipcBuffers);

   OSInitMessageQueue(virt_addrof(perCoreData->queue),
                      virt_addrof(perCoreData->messages),
                      static_cast<int32_t>(perCoreData->messages.size()));

   auto thread = virt_addrof(perCoreData->thread);
   auto stack = virt_addrof(perCoreData->stack);
   auto stackSize = perCoreData->stack.size();
   OSCreateThread(thread,
                  ipcDriverData->threadEntryPoint,
                  driver->coreId,
                  nullptr,
                  virt_cast<uint32_t *>(stack + stackSize),
                  static_cast<uint32_t>(stackSize),
                  -1,
                  static_cast<OSThreadAttributes>(1 << driver->coreId));

   switch (driver->coreId) {
   case 0:
      OSSetThreadName(thread, virt_addrof(ipcDriverData->threadName0));
      break;
   case 1:
      OSSetThreadName(thread, virt_addrof(ipcDriverData->threadName1));
      break;
   case 2:
      OSSetThreadName(thread, virt_addrof(ipcDriverData->threadName2));
      break;
   }

   OSResumeThread(thread);
}


/**
 * Open the IPC driver.
 *
 * \retval IOSError::OK
 * Success.
 *
 * \retval IOSError::NotReady
 * The IPC driver status must be Closed or Initialised.
 */
IOSError
IPCDriverOpen()
{
   auto driver = internal::getIPCDriver();

   // Verify driver state
   if (driver->status != IPCDriverStatus::Closed &&
       driver->status != IPCDriverStatus::Initialised) {
      return IOSError::NotReady;
   }

   // Initialise requests
   for (auto i = 0u; i < IPCBufferCount; ++i) {
      auto buffer = &driver->ipcBuffers[i];
      auto request = &driver->requests[i];
      request->ipcBuffer = buffer;
      request->asyncCallback = nullptr;
      request->asyncContext = nullptr;
   }

   driver->initialisedRequests = TRUE;

   // Initialise FIFO
   internal::ipcDriverFifoInit(&driver->freeFifo);
   internal::ipcDriverFifoInit(&driver->outboundFifo);

   // Push all items into free queue
   for (auto i = 0u; i < IPCBufferCount; ++i) {
      internal::ipcDriverFifoPush(&driver->freeFifo, &driver->requests[i]);
   }

   return IOSError::OK;
}


/**
 * Close the IPC driver.
 *
 * \retval IOSError::OK
 * Success.
 */
IOSError
IPCDriverClose()
{
   auto driver = internal::getIPCDriver();
   driver->status = IPCDriverStatus::Closed;
   return IOSError::OK;
}


namespace internal
{

/**
 * Get the IPC driver for the current core
 */
virt_ptr<IPCDriver>
getIPCDriver()
{
   return virt_addrof(getIpcDriverPerCoreData()->driver);
}


/**
 * Initialise IPCDriverFIFO
 */
void
ipcDriverFifoInit(virt_ptr<IPCDriverFIFO> fifo)
{
   fifo->pushIndex = 0;
   fifo->popIndex = -1;
   fifo->count = 0;
   fifo->maxCount = 0;

   for (auto i = 0u; i < IPCBufferCount; ++i) {
      fifo->requests[i] = nullptr;
   }
}


/**
 * Push a request into an IPCDriverFIFO structure
 *
 * \retval IOSError::OK Success
 * \retval IOSError::QFull There was no free space in the queue to push the request.
 */
IOSError
ipcDriverFifoPush(virt_ptr<IPCDriverFIFO> fifo,
                  virt_ptr<IPCDriverRequest> request)
{
   if (fifo->pushIndex == fifo->popIndex) {
      return IOSError::QFull;
   }

   fifo->requests[fifo->pushIndex] = request;

   if (fifo->popIndex == -1) {
      fifo->popIndex = fifo->pushIndex;
   }

   fifo->count += 1;
   fifo->pushIndex = (fifo->pushIndex + 1) % IPCBufferCount;

   if (fifo->count > fifo->maxCount) {
      fifo->maxCount = fifo->count;
   }

   return IOSError::OK;
}


/**
 * Pop a request into an IPCDriverFIFO structure.
 *
 * \retval IOSError::OK
 * Success
 *
 * \retval IOSError::QEmpty
 * There was no requests to pop from the queue.
 */
IOSError
ipcDriverFifoPop(virt_ptr<IPCDriverFIFO> fifo,
                 virt_ptr<IPCDriverRequest> *requestOut)
{
   if (fifo->popIndex == -1) {
      return IOSError::QEmpty;
   }

   auto request = fifo->requests[fifo->popIndex];
   fifo->count -= 1;

   if (fifo->count == 0) {
      fifo->popIndex = -1;
   } else {
      fifo->popIndex = (fifo->popIndex + 1) % IPCBufferCount;
   }

   *requestOut = request;
   return IOSError::OK;
}


/**
 * Allocates and initialises a IPCDriverRequest.
 *
 * This function can block with OSWaitEvent until there is a free request to
 * pop from the freeFifo queue.
 *
 * \return
 * Returns IOSError::OK on success, an IOSError code otherwise.
 */
IOSError
ipcDriverAllocateRequest(virt_ptr<IPCDriver> driver,
                         virt_ptr<IPCDriverRequest> *requestOut,
                         IOSHandle handle,
                         IOSCommand command,
                         uint32_t requestUnk0x04,
                         IOSAsyncCallbackFn asyncCallback,
                         virt_ptr<void> asyncContext)
{
   virt_ptr<IPCDriverRequest> request = nullptr;
   auto error = IOSError::OK;

   do {
      error = ipcDriverFifoPop(&driver->freeFifo, &request);

      if (error) {
         driver->failedAllocateRequestBlock += 1;

         if (error == IOSError::QEmpty) {
            driver->waitingFreeFifo = TRUE;
            OSWaitEvent(&driver->waitFreeFifoEvent);
         }
      }
   } while (error == IOSError::QEmpty);

   if (error != IOSError::OK) {
      return error;
   }

   request->allocated = TRUE;
   request->unk0x04 = requestUnk0x04;
   request->asyncCallback = asyncCallback;
   request->asyncContext = asyncContext;

   auto ipcBuffer = request->ipcBuffer;
   std::memset(virt_addrof(ipcBuffer->request).getRawPointer(), 0, sizeof(kernel::IpcRequest));
   ipcBuffer->request.command = command;
   ipcBuffer->request.handle = handle;
   ipcBuffer->request.flags = 0u;
   ipcBuffer->request.clientPid = 0;
   ipcBuffer->request.reply = IOSError::OK;

   *requestOut = request;
   return IOSError::OK;
}


/**
 * Free a IPCDriverRequest.
 *
 * \retval IOSError::OK
 * Success.
 *
 * \retval IOSError::QFull
 * The driver's freeFifo queue was full thus we were unable to free the request.
 */
IOSError
ipcDriverFreeRequest(virt_ptr<IPCDriver> driver,
                     virt_ptr<IPCDriverRequest> request)
{
   auto error = ipcDriverFifoPush(&driver->freeFifo, request);
   request->allocated = FALSE;

   if (error != IOSError::OK) {
      driver->failedFreeRequestBlock += 1;
   }

   return error;
}


/**
 * Submits an IPCDriverRequest to the kernel IPC driver.
 *
 * \retval IOSError::OK
 * Success.
 */
IOSError
ipcDriverSubmitRequest(virt_ptr<IPCDriver> driver,
                       virt_ptr<IPCDriverRequest> request)
{
   OSInitEvent(&request->finishEvent, FALSE, OSEventMode::AutoReset);
   driver->requestsSubmitted++;
   kernel::ipcDriverKernelSubmitRequest(virt_cast<kernel::IpcRequest *>(virt_addr { request->ipcBuffer.getAddress() }));
   return IOSError::OK;
}


/**
 * Blocks and waits for a response to an IPCDriverRequest.
 *
 * \return
 * Returns IOSError::OK or an IOSHandle on success, or an IOSError code otherwise.
 */
IOSError
ipcDriverWaitResponse(virt_ptr<IPCDriver> driver,
                      virt_ptr<IPCDriverRequest> request)
{
   OSWaitEvent(virt_addrof(request->finishEvent));

   auto response = request->ipcBuffer->request.reply;

   ipcDriverFreeRequest(driver, request);
   OSSignalEventAll(virt_addrof(driver->waitFreeFifoEvent));
   return response;
}


/**
 * Called by kernel IPC driver to indicate there are pending responses to process.
 */
void
ipcDriverProcessResponses()
{
   auto driver = getIPCDriver();
   auto coreData = &sIpcData->coreData[driver->coreId];

   for (auto i = 0u; i < driver->numResponses; ++i) {
      auto buffer = driver->responses[i];
      auto index = buffer.get() - driver->ipcBuffers.get();
      decaf_check(index >= 0);
      decaf_check(index <= IPCBufferCount);

      auto request = &driver->requests[index];
      decaf_check(request->ipcBuffer == buffer);

      if (!request->asyncCallback) {
         OSSignalEvent(virt_addrof(request->finishEvent));
      } else {
         OSMessage message;
         message.message = mem::translate<void>(request->asyncCallback.getAddress());
         message.args[0] = static_cast<uint32_t>(request->ipcBuffer->request.reply.value());
         message.args[1] = request->asyncContext.getAddress();
         message.args[2] = 0;
         OSSendMessage(virt_addrof(coreData->queue), &message, OSMessageFlags::None);
         ipcDriverFreeRequest(driver, request);
      }

      driver->requestsProcessed++;
      driver->responses[i] = nullptr;
   }

   driver->numResponses = 0;
}


static uint32_t
ipcDriverThreadEntry(uint32_t coreId,
                     void *arg2)
{
   StackObject<OSMessage> msg;
   auto coreData = &sIpcData->coreData[coreId];

   while (true) {
      OSReceiveMessage(virt_addrof(coreData->queue), msg, OSMessageFlags::Blocking);

      if (msg->args[2]) {
         // Received shutdown message
         break;
      }

      // Received callback message
      auto callback = IOSAsyncCallbackFn { msg->message.getAddress() };
      auto error = static_cast<IOSError>(msg->args[0]);
      auto context = virt_cast<void *>(static_cast<virt_addr>(msg->args[1]));
      callback(error, context);
   }

   IPCDriverClose();
   return 0;
}

} // namespace internal

void
Library::initialiseIpcDriverStaticData()
{
   auto ipcDriverData = allocStaticData<StaticIpcDriverData>();
   getStaticData()->ipcDriverData = ipcDriverData;

   ipcDriverData->threadEntryPoint = GetInternalFunctionAddress(internal::ipcDriverThreadEntry);
}

void
Library::registerIpcDriverFunctions()
{
   RegisterFunctionExport(IPCDriverInit);
   RegisterFunctionExport(IPCDriverOpen);
   RegisterFunctionExport(IPCDriverClose);

   RegisterFunctionInternal(internal::ipcDriverThreadEntry);
}

} // namespace cafe::coreinit

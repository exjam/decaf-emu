#include "coreinit.h"
#include "coreinit_appio.h"
#include "coreinit_fs.h"
#include "coreinit_fs_client.h"
#include "coreinit_fs_driver.h"
#include "coreinit_fastmutex.h"

namespace cafe::coreinit
{

using ClientBodyQueue = internal::Queue<FSClientBodyQueue, FSClientBodyLink,
                                        FSClientBody, &FSClientBody::link>;

struct StaticFsData
{
   be2_val<bool> initialised;
   be2_struct<OSFastMutex> mutex;
   be2_val<uint32_t> numClients;
   be2_struct<FSClientBodyQueue> clients;
};

static virt_ptr<StaticFsData>
getFsData()
{
   return Library::getStaticData()->fsData;
}


/**
 * Initialise filesystem.
 */
void
FSInit()
{
   auto fsData = getFsData();
   if (fsData->initialised || internal::fsDriverDone()) {
      return;
   }

   fsData->initialised = true;
   ClientBodyQueue::init(virt_addrof(fsData->clients));
   OSFastMutex_Init(virt_addrof(fsData->mutex), nullptr);
}


/**
 * Shutdown filesystem.
 */
void
FSShutdown()
{
}


/**
 * Get an FSAsyncResult from an OSMessage.
 */
virt_ptr<FSAsyncResult>
FSGetAsyncResult(virt_ptr<OSMessage> message)
{
   return virt_cast<FSAsyncResult *>(message->message);
}


/**
 * Get the number of registered FS clients.
 */
uint32_t
FSGetClientNum()
{
   return getFsData()->numClients;
}


namespace internal
{

/**
 * Returns true if filesystem has been intialised.
 */
bool
fsInitialised()
{
   return getFsData()->initialised;
}


/**
 * Returns true if client is registered.
 */
bool
fsClientRegistered(virt_ptr<FSClient> client)
{
   return fsClientRegistered(fsClientGetBody(client));
}


/**
 * Returns true if client is registered.
 */
bool
fsClientRegistered(virt_ptr<FSClientBody> clientBody)
{
   auto fsData = getFsData();
   OSFastMutex_Lock(virt_addrof(fsData->mutex));
   auto registered = ClientBodyQueue::contains(virt_addrof(fsData->clients),
                                               clientBody);
   OSFastMutex_Unlock(virt_addrof(fsData->mutex));
   return registered;
}


/**
 * Register a client with the filesystem.
 */
bool
fsRegisterClient(virt_ptr<FSClientBody> clientBody)
{
   auto fsData = getFsData();
   OSFastMutex_Lock(virt_addrof(fsData->mutex));
   ClientBodyQueue::append(virt_addrof(fsData->clients), clientBody);
   fsData->numClients++;
   OSFastMutex_Unlock(virt_addrof(fsData->mutex));
   return true;
}


/**
 * Deregister a client from the filesystem.
 */
bool
fsDeregisterClient(virt_ptr<FSClientBody> clientBody)
{
   auto fsData = getFsData();
   OSFastMutex_Lock(virt_addrof(fsData->mutex));
   ClientBodyQueue::erase(virt_addrof(fsData->clients), clientBody);
   fsData->numClients--;
   OSFastMutex_Unlock(virt_addrof(fsData->mutex));
   return true;
}


/**
 * Initialise an FSAsyncResult structure for an FS command.
 *
 * \retval FSStatus::OK
 * Success.
 */
FSStatus
fsAsyncResultInit(virt_ptr<FSClientBody> clientBody,
                  virt_ptr<FSAsyncResult> asyncResult,
                  virt_ptr<const FSAsyncData> asyncData)
{
   asyncResult->asyncData = *asyncData;

   if (!asyncData->ioMsgQueue) {
      asyncResult->asyncData.ioMsgQueue = OSGetDefaultAppIOQueue();
   }

   asyncResult->client = clientBody->client;
   asyncResult->ioMsg.data = asyncResult;
   asyncResult->ioMsg.type = OSFunctionType::FsCmdAsync;
   return FSStatus::OK;
}

} // namespace internal

void
Library::initialiseFsStaticData()
{
   auto fsData = allocStaticData<StaticFsData>();
   getStaticData()->fsData = fsData;
}

void
Library::registerFsFunctions()
{
   RegisterFunctionExport(FSInit);
   RegisterFunctionExport(FSShutdown);
   RegisterFunctionExport(FSGetAsyncResult);
   RegisterFunctionExport(FSGetClientNum);
}

} // namespace cafe::coreinit

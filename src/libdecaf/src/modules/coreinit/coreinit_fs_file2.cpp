#include "coreinit_fs.h"
#include "common/align.h"
#include "common/log.h"

namespace coreinit
{

FSClientData *
getClientData(FSClient *client)
{
   return mem::translate<FSClientData>(align_up(mem::untranslate(client), 0x40));
}

FSCmdBlockData *
getCmdBlockData(FSCmdBlock *block)
{
   return mem::translate<FSCmdBlockData>(align_up(mem::untranslate(block), 0x40));
}

FSStatus
prepareCmd(FSClientData *clientData, FSCmdBlockData *cmd, uint32_t flags, FSAsyncData *asyncData)
{
   decaf_check(clientData);
   decaf_check(cmd);

   if (cmd->status != FSCmdBlockStatus::Initialised && cmd->status != FSCmdBlockStatus::Cancelled) {
      gLog->error("Invalid FSCmdBlockData state {}", cmd->status.value());
      return FSStatus::FatalError;
   }

   if (asyncData->callback && asyncData->queue) {
      gLog->error("Both callback and queue set for FSAsyncData");
      return FSStatus::FatalError;
   }

   cmd->flags = flags;
   cmd->clientData = clientData;

   // cmd + 0x940 = clrrwi *(cmd + 0x94) & ~1, 1
   // cmd + 0x9e9 = byte 0

   // asyncResultInit(r3 = client, r4 = result, r5 = asyncData)
   cmd->asyncResult.userParams = *asyncData;

   if (cmd->asyncResult.userParams.callback) {
      // cmd->asyncResult.userParams.queue = OSGetDefaultAppIOQueue();
   }

   cmd->asyncResult.ioMsg.message = &cmd->asyncResult;
   cmd->asyncResult.ioMsg.args[2] = 8;
   cmd->asyncResult.client = clientData->client;

   return FSStatus::OK;
}


FSStatus
FSTruncateFileAsync(FSClient *client,
                    FSCmdBlock *block,
                    FSFileHandle handle,
                    uint32_t flags,
                    FSAsyncData *asyncData)
{
   auto clientData = getClientData(client);
   auto cmdData = getCmdBlockData(block);
   auto status = prepareCmd(clientData, cmdData, flags, asyncData);

   if (status != FSStatus::OK) {
      return status;
   }
   /*
   internal::queueFsWork(client, block, asyncData, [=]() {
      auto file = client->getOpenFile(handle);

      if (!file) {
         return FSStatus::FatalError;
      }

      file->truncate();
      return FSStatus::OK;
   });
   */
   return FSStatus::OK;
}

FSStatus
FSTruncateFile(FSClient *client,
               FSCmdBlock *block,
               FSFileHandle handle,
               uint32_t flags)
{
   auto asyncData = internal::prepareSyncOp(client, block);
   FSTruncateFileAsync(client, block, handle, flags, asyncData);
   return internal::resolveSyncOp(client, block);
}

} // namespace coreinit

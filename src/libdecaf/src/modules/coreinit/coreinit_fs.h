#pragma once
#include "common/types.h"
#include "coreinit_enum.h"
#include "coreinit_messagequeue.h"
#include "common/be_val.h"
#include "common/structsize.h"
#include "ppcutils/wfunc_ptr.h"
#include <functional>

namespace coreinit
{

/**
 * \defgroup coreinit_fs Filesystem
 * \ingroup coreinit
 */

using FSDirectoryHandle = uint32_t;
using FSFileHandle = uint32_t;
using FSPriority = uint32_t;

/*
Unimplemented filesystem functions:
FSAppendFile
FSAppendFileAsync
FSBindMount
FSBindMountAsync
FSBindUnmount
FSBindUnmountAsync
FSCancelAllCommands
FSCancelCommand
FSChangeMode
FSChangeModeAsync
FSDumpLastErrorLog
FSFlushFile
FSFlushFileAsync
FSFlushMultiQuota
FSFlushMultiQuotaAsync
FSFlushQuota
FSFlushQuotaAsync
FSGetAsyncResult
FSGetCmdPriority
FSGetCurrentCmdBlock
FSGetCwdAsync
FSGetDirSize
FSGetDirSizeAsync
FSGetEmulatedError
FSGetEntryNum
FSGetEntryNumAsync
FSGetFSMessage
FSGetFileBlockAddress
FSGetFileBlockAddressAsync
FSGetFileSystemInfo
FSGetFileSystemInfoAsync
FSGetFreeSpaceSize
FSGetFreeSpaceSizeAsync
FSGetLastError
FSGetMountSource
FSGetMountSourceAsync
FSGetMountSourceNext
FSGetMountSourceNextAsync
FSGetStateChangeInfo
FSGetUserData
FSGetVolumeInfo
FSGetVolumeInfoAsync
FSMakeLink
FSMakeLinkAsync
FSMakeQuota
FSMakeQuotaAsync
FSMount
FSMountAsync
FSOpenFileByStat
FSOpenFileByStatAsync
FSOpenFileEx
FSOpenFileExAsync
FSRegisterFlushQuota
FSRegisterFlushQuotaAsync
FSRemove
FSRemoveAsync
FSRemoveQuota
FSRemoveQuotaAsync
FSRename
FSRenameAsync
FSRollbackQuota
FSRollbackQuotaAsync
FSSetEmulatedError
FSSetUserData
FSTimeToCalendarTime
FSUnmount
FSUnmountAsync
*/

#pragma pack(push, 1)

class FSClient;
struct FSCmdBlock;

using FSAsyncCallback = wfunc_ptr<void, FSClient *, FSCmdBlock *, FSStatus, uint32_t>;
using be_FSAsyncCallback = be_wfunc_ptr<void, FSClient *, FSCmdBlock *, FSStatus, uint32_t>;

struct FSAsyncData
{
   be_val<uint32_t> callback;
   be_val<uint32_t> param;
   be_ptr<OSMessageQueue> queue;
};
CHECK_OFFSET(FSAsyncData, 0x00, callback);
CHECK_OFFSET(FSAsyncData, 0x04, param);
CHECK_OFFSET(FSAsyncData, 0x08, queue);
CHECK_SIZE(FSAsyncData, 0xC);

struct FSAsyncResult
{
   FSAsyncData userParams;
   OSMessage ioMsg;
   be_ptr<FSClient> client;
   be_ptr<FSCmdBlock> block;
   FSStatus status;
};
CHECK_OFFSET(FSAsyncResult, 0x00, userParams);
CHECK_OFFSET(FSAsyncResult, 0x0c, ioMsg);
CHECK_OFFSET(FSAsyncResult, 0x1c, client);
CHECK_OFFSET(FSAsyncResult, 0x20, block);
CHECK_OFFSET(FSAsyncResult, 0x24, status);
CHECK_SIZE(FSAsyncResult, 0x28);


enum FSCmdBlockStatus : uint32_t
{
   Initialised = 0xD900A21,
   Cancelled = 0xD900A24,
};

enum FSCmdBlockCommand : uint32_t
{
   OpenFile = 0xE,
   ReadFile = 0xF,
   WriteFile = 0x10,
   TruncateFile = 0x1A,
};

// FSClientData = align_up(FSClient, 0x40)
struct FSClientData
{
   // 0x1444 - uint32_t client handle (from IOS)
   // 0x1448 - fsm init ???
   // 0x1480 - command queue
   // 0x1560 - mutex
   // 0x1590 - alarm

   // 0x1614 - clientlist linked list prev
   // 0x1618 - clientlist linked list next
   // 0x161c - FSClient * base pointer
   UNKNOWN(0x161C);
   be_ptr<FSClient> client;
};

// FSCmdBlockData = align_up(FSCmdBlock, 0x40)
struct FSCmdBlockData
{
   /*
   FSOpenFile(path, mode, filehandle)
      - 0x944 FileHandle

      // __FSAShimSetupRequestOpenFile
      - commandID = 0xE
      - 0x904 = lwz 0x1444(FSClientBody)
      - 0x584 = -1

      // u wot m8 lel?
      - 0x290 = 0x660 <- permissions?
      - 0x294 = 0
      - 0x298 = 0

   FSWriteFileWithPos(r5 = buffer, r6 = size, r7 = count, r8 = pos, r9 = filehandle, r10 = flags?)
      // This shit is to repeatedly queue command cos can only do 0x40000 at a time!!
      // exists for ReadFile too
      - 0x948 = size * count
      - 0x94c = 0
      - 0x950 = size
      - 0x954 = std::min(size * count, 0x40000)

      __FSAShimSetupRequestWriteFile(cmd, lwz 0x1444(client), buffer, 1, std::min(size * count, 0x40000), pos, filehandle, flags)

   __FSAShimSetupRequestWriteFile(FSCmdBlockData *cmd, uint32_t unk, void *buffer, uint32_t blocks, uint32_t blockSize, pos, filehandle, flags)
      - 0x4 = buffer
      - 0x8 = blocks
      - 0xc = blockSize
      - 0x10 = pos
      - 0x14 = filehandle
      - 0x18 = flags

      - 0x880 = cmd
      - 0x884 = 0x520
      - 0x88C = buffer
      - 0x890 = blocks
      - 0x898 = cmd + 0x580
      - 0x89C = 0x293

      - 0x900 - commandID = 0x10
      - 0x904 = unk (lwz 0x1444(FSClientBody))
      - 0x908 = half 1
      - 0x90A = byte 2
      - 0x90B = byte 1

   __FSAShimSetupRequestTruncateFile(FSCmdBlockData *cmd, uint32_t unk, uint32_t filehandle)
      - 0x4 = filehandle
      - 0x900 = command = 0x1A
      - 0x904 = unk (lwz 0x1444(FSClientBody)) client Handle / ID
      - 0x908 = half 0
   */
   UNKNOWN(4);
   char path[0x280];
   char mode[0x10];
   UNKNOWN(0x900 - 0x294);
   be_val<uint32_t> commandID;
   UNKNOWN(0x938 - 0x904);
   be_ptr<FSClientData> clientData;
   be_val<FSCmdBlockStatus> status;
   UNKNOWN(0x968 - 0x940);
   be_val<uint32_t> flags;
   FSAsyncResult asyncResult;
   be_ptr<void> userData;
   OSMessageQueue syncQueue;
   OSMessage syncQueueMsgs[1];
   UNKNOWN(4);
   be_val<uint8_t> priority; // 0 - 0x20, default = 0x10
};
CHECK_OFFSET(FSCmdBlockData, 0x004, path);
CHECK_OFFSET(FSCmdBlockData, 0x284, mode);
CHECK_OFFSET(FSCmdBlockData, 0x900, commandID);
CHECK_OFFSET(FSCmdBlockData, 0x938, clientData);
CHECK_OFFSET(FSCmdBlockData, 0x93C, status);
CHECK_OFFSET(FSCmdBlockData, 0x968, flags);
CHECK_OFFSET(FSCmdBlockData, 0x96C, asyncResult);
CHECK_OFFSET(FSCmdBlockData, 0x994, userData);
CHECK_OFFSET(FSCmdBlockData, 0x998, syncQueue);
CHECK_OFFSET(FSCmdBlockData, 0x9D4, syncQueueMsgs);
CHECK_OFFSET(FSCmdBlockData, 0x9E8, priority);

struct FSCmdBlock
{
   static const unsigned MaxPathLength = 0x280;
   static const unsigned MaxModeLength = 0x10;

   // HACK: We store our own stuff into PPC memory...  This is
   //  especially bad as std::function is not really meant to be
   //  randomly memset...
   uint32_t priority;
   FSAsyncResult result;
   std::function<FSStatus()> func;
   OSMessageQueue syncQueue;
   OSMessage syncQueueMsgs[1];
   char path[MaxPathLength];
   char mode[MaxModeLength];
   UNKNOWN(0x778 - sizeof(std::function<FSStatus()>));
};
CHECK_SIZE(FSCmdBlock, 0xa80);

struct FSStat
{
   enum Flags
   {
      Directory = 0x80000000,
   };

   be_val<uint32_t> flags;
   UNKNOWN(0xc);
   be_val<uint32_t> size;
   UNKNOWN(0x50);
};
CHECK_OFFSET(FSStat, 0x00, flags);
CHECK_OFFSET(FSStat, 0x10, size);
CHECK_SIZE(FSStat, 0x64);

struct FSStateChangeInfo
{
   UNKNOWN(0xC);
};
CHECK_SIZE(FSStateChangeInfo, 0xC);

struct FSDirectoryEntry
{
   FSStat info;
   char name[256];
};
CHECK_OFFSET(FSDirectoryEntry, 0x64, name);
CHECK_SIZE(FSDirectoryEntry, 0x164);

#pragma pack(pop)

void
FSInit();

void
FSShutdown();

void
FSInitCmdBlock(FSCmdBlock *block);

FSStatus
FSSetCmdPriority(FSCmdBlock *block,
                 FSPriority priority);

/** @} */

namespace internal
{

void
startFsThread();

void
shutdownFsThread();

void
handleFsDoneInterrupt();

FSAsyncData *
prepareSyncOp(FSClient *client,
              FSCmdBlock *block);

FSStatus
resolveSyncOp(FSClient *client,
              FSCmdBlock *block);

void
queueFsWork(FSClient *client,
            FSCmdBlock *block,
            FSAsyncData *asyncData,
            std::function<FSStatus()> func);

bool
cancelFsWork(FSCmdBlock *cmd);

void
cancelAllFsWork();

} // namespace internal

} // namespace coreinit

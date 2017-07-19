#ifndef IOS_ENUM_H
#define IOS_ENUM_H

#include <common/enum_start.h>

ENUM_NAMESPACE_BEG(ios)

ENUM_BEG(IOSCommand, int32_t)
   ENUM_VALUE(Open,                 1)
   ENUM_VALUE(Close,                2)
   ENUM_VALUE(Read,                 3)
   ENUM_VALUE(Write,                4)
   ENUM_VALUE(Seek,                 5)
   ENUM_VALUE(Ioctl,                6)
   ENUM_VALUE(Ioctlv,               7)
   ENUM_VALUE(Reply,                8)
ENUM_END(IOSCommand)

ENUM_BEG(IOSCpuId, uint32_t)
   ENUM_VALUE(ARM,                  0)
   ENUM_VALUE(PPC0,                 1)
   ENUM_VALUE(PPC1,                 2)
   ENUM_VALUE(PPC2,                 3)
ENUM_END(IOSCpuId)

ENUM_BEG(IOSErrorCategory, uint32_t)
   ENUM_VALUE(Kernel,               0)
   ENUM_VALUE(FSA,                  3)
   ENUM_VALUE(MCP,                  4)
   ENUM_VALUE(Unknown7,             7)
   ENUM_VALUE(Unknown8,             8)
   ENUM_VALUE(Socket,               10)
   ENUM_VALUE(ODM,                  14)
   ENUM_VALUE(Unknown15,            15)
   ENUM_VALUE(Unknown19,            19)
   ENUM_VALUE(Unknown30,            30)
   ENUM_VALUE(Unknown45,            45)
ENUM_END(IOSErrorCategory)

ENUM_BEG(IOSError, int32_t)
   ENUM_VALUE(OK,                   0)
   ENUM_VALUE(Access,               -1)
   ENUM_VALUE(Exists,               -2)
   ENUM_VALUE(Intr,                 -3)
   ENUM_VALUE(Invalid,              -4)
   ENUM_VALUE(Max,                  -5)
   ENUM_VALUE(NoExists,             -6)
   ENUM_VALUE(QEmpty,               -7)
   ENUM_VALUE(QFull,                -8)
   ENUM_VALUE(Unknown,              -9)
   ENUM_VALUE(NotReady,             -10)
   ENUM_VALUE(Ecc,                  -11)
   ENUM_VALUE(EccCrit,              -12)
   ENUM_VALUE(BadBlock,             -13)
   ENUM_VALUE(InvalidObjType,       -14)
   ENUM_VALUE(InvalidRNG,           -15)
   ENUM_VALUE(InvalidFlag,          -16)
   ENUM_VALUE(InvalidFormat,        -17)
   ENUM_VALUE(InvalidVersion,       -18)
   ENUM_VALUE(InvalidSigner,        -19)
   ENUM_VALUE(FailCheckValue,       -20)
   ENUM_VALUE(FailInternal,         -21)
   ENUM_VALUE(FailAlloc,            -22)
   ENUM_VALUE(InvalidSize,          -23)
   ENUM_VALUE(NoLink,               -24)
   ENUM_VALUE(ANFailed,             -25)
   ENUM_VALUE(MaxSemCount,          -26)
   ENUM_VALUE(SemUnavailable,       -27)
   ENUM_VALUE(InvalidHandle,        -28)
   ENUM_VALUE(InvalidArg,           -29)
   ENUM_VALUE(NoResource,           -30)
   ENUM_VALUE(Busy,                 -31)
   ENUM_VALUE(Timeout,              -32)
   ENUM_VALUE(Alignment,            -33)
   ENUM_VALUE(BSP,                  -34)
   ENUM_VALUE(DataPending,          -35)
   ENUM_VALUE(Expired,              -36)
   ENUM_VALUE(NoReadAccess,         -37)
   ENUM_VALUE(NoWriteAccess,        -38)
   ENUM_VALUE(NoReadWriteAccess,    -39)
   ENUM_VALUE(ClientTxnLimit,       -40)
   ENUM_VALUE(StaleHandle,          -41)
   ENUM_VALUE(UnknownValue,         -42)
   ENUM_VALUE(MaxKernelError,       -0x400)
ENUM_END(IOSError)

ENUM_BEG(IOSOpenMode, int32_t)
   ENUM_VALUE(None,                 0)
   ENUM_VALUE(Read,                 1)
   ENUM_VALUE(Write,                2)
   ENUM_VALUE(ReadWrite,            Read | Write)
ENUM_END(IOSOpenMode)

ENUM_BEG(IOSProcessID, uint32_t)
   ENUM_VALUE(KERNEL,               0)
   ENUM_VALUE(MCP,                  1)
   ENUM_VALUE(BSP,                  2)
   ENUM_VALUE(CRYPTO,               3)
   ENUM_VALUE(USB,                  4)
   ENUM_VALUE(FS,                   5)
   ENUM_VALUE(PAD,                  6)
   ENUM_VALUE(NET,                  7)
   ENUM_VALUE(ACP,                  8)
   ENUM_VALUE(NSEC,                 9)
   ENUM_VALUE(AUXIL,                10)
   ENUM_VALUE(NIM,                  11)
   ENUM_VALUE(FPD,                  12)
   ENUM_VALUE(IOSTEST,              13)
   ENUM_VALUE(COSKERNEL,            14)
   ENUM_VALUE(COSROOT,              15)
   ENUM_VALUE(COS02,                16)
   ENUM_VALUE(COS03,                17)
   ENUM_VALUE(COSOVERLAY,           18)
   ENUM_VALUE(COSHBM,               19)
   ENUM_VALUE(COSERROR,             20)
   ENUM_VALUE(COSMASTER,            21)
   ENUM_VALUE(Max,                  22)
ENUM_END(IOSProcessID)

ENUM_BEG(IOSSyscall, uint32_t)
   ENUM_VALUE(CreateThread,            0x00)
   ENUM_VALUE(JoinThread,              0x01)
   ENUM_VALUE(CancelThread,            0x02)
   ENUM_VALUE(GetCurrentThreadID,      0x03)

   ENUM_VALUE(GetCurrentProcessID,     0x05)
   ENUM_VALUE(GetCurrentProcessName,   0x06)
   ENUM_VALUE(StartThread,             0x07)
   ENUM_VALUE(SuspendThread,           0x08)
   ENUM_VALUE(YieldThread,             0x09)
   ENUM_VALUE(GetThreadPriority,       0x0A)
   ENUM_VALUE(SetThreadPriority,       0x0B)
   ENUM_VALUE(CreateMessageQueue,      0x0C)
   ENUM_VALUE(DestroyMessageQueue,     0x0D)
   ENUM_VALUE(SendMessage,             0x0E)
   ENUM_VALUE(JamMessage,              0x0F)
   ENUM_VALUE(ReceiveMessage,          0x10)
   ENUM_VALUE(HandleEvent,             0x11)
   ENUM_VALUE(UnhandleEvent,           0x12)
   ENUM_VALUE(CreateTimer,             0x13)
   ENUM_VALUE(RestartTimer,            0x14)
   ENUM_VALUE(StopTimer,               0x15)
   ENUM_VALUE(DestroyTimer,            0x16)

   ENUM_VALUE(GetUpTimeStruct,         0x19)
   ENUM_VALUE(GetUpTime64,             0x1A)

   ENUM_VALUE(GetAbsTimeCalendar,      0x1C)
   ENUM_VALUE(GetAbsTime64,            0x1D)
   ENUM_VALUE(GetAbsTimeStruct,        0x1E)

   ENUM_VALUE(CreateLocalProcessHeap,  0x24)
   ENUM_VALUE(CreateCrossProcessHeap,  0x25)

   ENUM_VALUE(Alloc,                   0x27)
   ENUM_VALUE(AllocAligned,            0x28)
   ENUM_VALUE(Free,                    0x29)
   ENUM_VALUE(FreeAndZero,             0x2A)
   ENUM_VALUE(Realloc,                 0x2B)
   ENUM_VALUE(RegisterResourceManager, 0x2C)

   ENUM_VALUE(Open,                    0x33)
   ENUM_VALUE(Close,                   0x34)
   ENUM_VALUE(Read,                    0x35)
   ENUM_VALUE(Write,                   0x36)
   ENUM_VALUE(Seek,                    0x37)
   ENUM_VALUE(Ioctl,                   0x38)
   ENUM_VALUE(Ioctlv,                  0x39)
   ENUM_VALUE(OpenAsync,               0x3A)
   ENUM_VALUE(CloseAsync,              0x3B)
   ENUM_VALUE(ReadAsync,               0x3C)
   ENUM_VALUE(WriteAsync,              0x3D)
   ENUM_VALUE(SeekAsync,               0x3E)
   ENUM_VALUE(IoctlAsync,              0x3F)
   ENUM_VALUE(IoctlvAsync,             0x40)

   ENUM_VALUE(ResourceReply,           0x49)

   ENUM_VALUE(ClearAndEnable,          0x50)
   ENUM_VALUE(InvalidateDCache,        0x51)
ENUM_END(IOSSyscall)

ENUM_BEG(ThreadState, uint32_t)
   ENUM_VALUE(Available,               0x00)
   ENUM_VALUE(Ready,                   0x01)
   ENUM_VALUE(Running,                 0x02)
   ENUM_VALUE(Stopped,                 0x03)
   ENUM_VALUE(Waiting,                 0x04)
   ENUM_VALUE(Dead,                    0x05)
   ENUM_VALUE(Faulted,                 0x06)
   ENUM_VALUE(Unknown,                 0x07)
ENUM_END(ThreadState)

ENUM_NAMESPACE_END(ios)

#include <common/enum_end.h>

#endif // ifdef IOS_ENUM_H

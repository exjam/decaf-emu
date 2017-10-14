#pragma once
#include <cstdint>

namespace cafe::kernel
{

constexpr auto NumProcesses = 8;

enum class RamProcessId : int32_t
{
   Invalid = -1,
   Kernel = 0,
   Root = 1,
   OverlayApp = 4,
   HomeMenu = 5,
   ErrorDisplay = 6,
   MainApplication = 7,
};

enum class UniqueProcessId : int32_t
{
   Invalid = -1,
   Kernel = 0,
   Root = 1,
   HomeMenu = 2,
   TV = 3,
   EManual = 4,
   OverlayMenu = 5,
   ErrorDisplay = 6,
   MiniMiiverse = 7,
   InternetBrowser = 8,
   Miiverse = 9,
   EShop = 10,
   FLV = 11,
   DownloadManager = 12,
   Game = 15
};

enum class KernelProcessId : int32_t
{
   Invalid = -1,
   Kernel = 0,
   Loader = 2,
};

RamProcessId
getRampidFromUpid(UniqueProcessId id);

RamProcessId
getRampid();

KernelProcessId
getKernelProcessId();

void
switchToProcess(UniqueProcessId id);

} // namespace cafe::kernel

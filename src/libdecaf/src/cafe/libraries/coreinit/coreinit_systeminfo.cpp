#include "coreinit.h"
#include "coreinit_systeminfo.h"
#include "coreinit_time.h"

#include <chrono>
#include <common/platform_time.h>

namespace cafe::coreinit
{

struct StaticSystemInfoData
{
   be2_struct<OSSystemInfo> systemInfo;
   be2_val<BOOL> screenCapturePermission;
   be2_val<BOOL> enableHomeButtonMenu;
   be2_val<kernel::UniqueProcessId> uniqueProcessId;
   be2_val<OSAppFlags> appFlags;
   be2_val<uint64_t> titleID;
   be2_val<uint64_t> osID;
};

static virt_ptr<StaticSystemInfoData>
getSystemInfoData()
{
   return Library::getStaticData()->systemInfoData;
}

virt_ptr<OSSystemInfo>
OSGetSystemInfo()
{
   auto systemInfoData = getSystemInfoData();
   return virt_addrof(systemInfoData->systemInfo);
}

BOOL
OSSetScreenCapturePermission(BOOL enabled)
{
   auto systemInfoData = getSystemInfoData();
   auto old = systemInfoData->screenCapturePermission;
   systemInfoData->screenCapturePermission = enabled;
   return old;
}

BOOL
OSGetScreenCapturePermission()
{
   auto systemInfoData = getSystemInfoData();
   return systemInfoData->screenCapturePermission;
}

uint32_t
OSGetConsoleType()
{
   // Value from a WiiU retail v4.0.0
   return 0x3000050;
}

uint32_t
OSGetSecurityLevel()
{
   return 0;
}

BOOL
OSIsHomeButtonMenuEnabled()
{
   auto systemInfoData = getSystemInfoData();
   return systemInfoData->enableHomeButtonMenu;
}

BOOL
OSEnableHomeButtonMenu(BOOL enable)
{
   auto systemInfoData = getSystemInfoData();
   systemInfoData->enableHomeButtonMenu = enable;
   return TRUE;
}

void
OSBlockThreadsOnExit()
{
   // TODO: OSBlockThreadsOnExit
}

uint64_t
OSGetTitleID()
{
   return getSystemInfoData()->titleID;
}

uint64_t
OSGetOSID()
{
   return getSystemInfoData()->osID;
}

kernel::UniqueProcessId
OSGetUPID()
{
   return getSystemInfoData()->uniqueProcessId;
}

OSAppFlags
OSGetAppFlags()
{
   return getSystemInfoData()->appFlags;
}

OSShutdownReason
OSGetShutdownReason()
{
   return OSShutdownReason::NoShutdown;
}

void
OSGetArgcArgv(virt_ptr<uint32_t> argc,
              virt_ptr<virt_ptr<const char>> argv)
{
   *argc = 0u;
   *argv = nullptr;
}

namespace internal
{

bool
isAppDebugLevelVerbose()
{
   return getSystemInfoData()->appFlags.value()
      .debugLevel() >= OSAppFlagsDebugLevel::Verbose;
}

bool
isAppDebugLevelUnknown3()
{
   return getSystemInfoData()->appFlags.value()
      .debugLevel() >= OSAppFlagsDebugLevel::Unknown3;
}

} // namespace internal

void
Library::initialiseSystemInfoStaticData()
{
   auto systemInfoData = allocStaticData<StaticSystemInfoData>();
   getStaticData()->systemInfoData = systemInfoData;

   systemInfoData->systemInfo.busSpeed = cpu::busClockSpeed;
   systemInfoData->systemInfo.coreSpeed = cpu::coreClockSpeed;
   systemInfoData->systemInfo.baseTime = internal::getBaseTime();
   systemInfoData->systemInfo.l2CacheSize[0] = 512 * 1024u;
   systemInfoData->systemInfo.l2CacheSize[1] = 2 * 1024 * 1024u;
   systemInfoData->systemInfo.l2CacheSize[2] = 512 * 1024u;
   systemInfoData->systemInfo.cpuRatio = 5u;

   systemInfoData->screenCapturePermission = TRUE;
   systemInfoData->enableHomeButtonMenu = TRUE;
}

void
Library::registerSystemInfoExports()
{
   RegisterFunctionExport(OSGetSystemInfo);
   RegisterFunctionExport(OSSetScreenCapturePermission);
   RegisterFunctionExport(OSGetScreenCapturePermission);
   RegisterFunctionExport(OSGetConsoleType);
   RegisterFunctionExport(OSGetSecurityLevel);
   RegisterFunctionExport(OSEnableHomeButtonMenu);
   RegisterFunctionExport(OSIsHomeButtonMenuEnabled);
   RegisterFunctionExport(OSBlockThreadsOnExit);
   RegisterFunctionExport(OSGetTitleID);
   RegisterFunctionExport(OSGetOSID);
   RegisterFunctionExport(OSGetUPID);
   RegisterFunctionExport(OSGetShutdownReason);
   RegisterFunctionExport(OSGetArgcArgv);
   RegisterFunctionExportName("_OSGetAppFlags", OSGetAppFlags);
}

} // namespace cafe::coreinit

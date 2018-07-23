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
sSystemInfoData = nullptr;

virt_ptr<OSSystemInfo>
OSGetSystemInfo()
{
   return virt_addrof(sSystemInfoData->systemInfo);
}

BOOL
OSSetScreenCapturePermission(BOOL enabled)
{
   auto old = sSystemInfoData->screenCapturePermission;
   sSystemInfoData->screenCapturePermission = enabled;
   return old;
}

BOOL
OSGetScreenCapturePermission()
{
   return sSystemInfoData->screenCapturePermission;
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
   return sSystemInfoData->enableHomeButtonMenu;
}

BOOL
OSEnableHomeButtonMenu(BOOL enable)
{
   sSystemInfoData->enableHomeButtonMenu = enable;
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
   return sSystemInfoData->titleID;
}

uint64_t
OSGetOSID()
{
   return sSystemInfoData->osID;
}

kernel::UniqueProcessId
OSGetUPID()
{
   return sSystemInfoData->uniqueProcessId;
}

OSAppFlags
OSGetAppFlags()
{
   return sSystemInfoData->appFlags;
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

BOOL
OSIsDebuggerPresent()
{
   return FALSE;
}

BOOL
OSIsDebuggerInitialized()
{
   return FALSE;
}

int32_t
ENVGetEnvironmentVariable(virt_ptr<const char> key,
                          virt_ptr<char> buffer,
                          uint32_t length)
{
   if (buffer) {
      *buffer = char { 0 };
   }

   return 0;
}

namespace internal
{

bool
isAppDebugLevelVerbose()
{
   return sSystemInfoData->appFlags.value()
      .debugLevel() >= OSAppFlagsDebugLevel::Verbose;
}

bool
isAppDebugLevelUnknown3()
{
   return sSystemInfoData->appFlags.value()
      .debugLevel() >= OSAppFlagsDebugLevel::Unknown3;
}

void
initialiseSystemInfo()
{
   sSystemInfoData->systemInfo.busSpeed = cpu::busClockSpeed;
   sSystemInfoData->systemInfo.coreSpeed = cpu::coreClockSpeed;
   sSystemInfoData->systemInfo.baseTime = internal::getBaseTime();
   sSystemInfoData->systemInfo.l2CacheSize[0] = 512 * 1024u;
   sSystemInfoData->systemInfo.l2CacheSize[1] = 2 * 1024 * 1024u;
   sSystemInfoData->systemInfo.l2CacheSize[2] = 512 * 1024u;
   sSystemInfoData->systemInfo.cpuRatio = 5u;

   sSystemInfoData->screenCapturePermission = TRUE;
   sSystemInfoData->enableHomeButtonMenu = TRUE;
}

} // namespace internal

constexpr auto x = detail::is_gpr32_type<OSAppFlags>::value;

void
Library::registerSystemInfoSymbols()
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
   RegisterFunctionExport(OSIsDebuggerPresent);
   RegisterFunctionExport(OSIsDebuggerInitialized);
   RegisterFunctionExport(ENVGetEnvironmentVariable);
   RegisterFunctionExportName("_OSGetAppFlags", OSGetAppFlags);

   RegisterDataInternal(sSystemInfoData);
}

} // namespace cafe::coreinit

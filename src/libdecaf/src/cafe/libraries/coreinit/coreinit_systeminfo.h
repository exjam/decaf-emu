#pragma once
#include "coreinit_enum.h"
#include "coreinit_time.h"
#include "cafe/kernel/cafe_kernel_process.h"

#include <common/bitfield.h>
#include <libcpu/be2_struct.h>

namespace cafe::coreinit
{

#pragma pack(push, 1)

struct OSSystemInfo
{
   be2_val<uint32_t> busSpeed;
   be2_val<uint32_t> coreSpeed;
   be2_val<OSTime> baseTime;
   be2_array<uint32_t, 3> l2CacheSize;
   be2_val<uint32_t> cpuRatio;
};
CHECK_OFFSET(OSSystemInfo, 0x0, busSpeed);
CHECK_OFFSET(OSSystemInfo, 0x4, coreSpeed);
CHECK_OFFSET(OSSystemInfo, 0x8, baseTime);
CHECK_OFFSET(OSSystemInfo, 0x10, l2CacheSize);
CHECK_OFFSET(OSSystemInfo, 0x1C, cpuRatio);
CHECK_SIZE(OSSystemInfo, 0x20);

BITFIELD(OSAppFlags, uint32_t)
   BITFIELD_ENTRY(9, 3, OSAppFlagsDebugLevel, debugLevel);
BITFIELD_END

#pragma pack(pop)

virt_ptr<OSSystemInfo>
OSGetSystemInfo();

BOOL
OSSetScreenCapturePermission(BOOL enabled);

BOOL
OSGetScreenCapturePermission();

uint32_t
OSGetConsoleType();

uint32_t
OSGetSecurityLevel();

BOOL
OSEnableHomeButtonMenu(BOOL enable);

BOOL
OSIsHomeButtonMenuEnabled();

void
OSBlockThreadsOnExit();

uint64_t
OSGetTitleID();

uint64_t
OSGetOSID();

kernel::UniqueProcessId
OSGetUPID();

OSAppFlags
OSGetAppFlags();

OSShutdownReason
OSGetShutdownReason();

void
OSGetArgcArgv(virt_ptr<uint32_t> argc,
              virt_ptr<virt_ptr<const char>> argv);

namespace internal
{

bool
isAppDebugLevelVerbose();

bool
isAppDebugLevelUnknown3();

} // namespace internal

} // namespace cafe::coreinit

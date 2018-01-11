/*

Kernel initialisation leads to loader.__LoaderStart via KiRPLStartup

__LoaderStart
  - Setup heaps
  - LOADER_Init loads root.rpx
    - Setup local heap
    - Init IPCL driver
    - LiInitSharedForProcess
    - LiBounceOneChunk
    - LiLoadForPrep
    - LiLoadCoreIntoProcess
  - Loader_FinishInitAndPreload

root.rpx entry point
  - Does boring shit like display boot screen tga and sound
  - After that will try boot master title

root.rpx calls coreinit.OSSendAppSwitchRequest(0,0,0) (switch to kernel??? maybe 0,0,0 makes it launch master title?)
coreinit.OSSendAppSwitchRequest does syscall 0x2A00 to kernel
  - Kernel syscall 0x2A00 AppSwitchRequest
     -> 0xFFF15D84 -> MCP_PrepareTitle
     -> setProcessStateFlags 0x40
  - Kernel syscall 0x6100 QuerySwitchReady
     -> Check if received reply to PrepareTitle, return (r3:0 r4:1) if still waiting on reply
     -> Synchronous MCP_Ioctl0x51
     -> setProcessStateFlags 0x80
  - sCheckSysEvents which does syscall 0x2E GetSystemMessage
     - Kernel syscall 0x2E00 GetSystemMessage
        -> check gProcessStateFlags for 0xC0 { 0x80 / 0x40 } returns RELEASE
     -> syscall returns msg
     -> 0xBADBEEF = nothing
     -> 0xDEF0DEF = nothing
     -> OSSendMessage(OSGetSystemMessageQueue(), sMessageToMsg(msg))

root.rpx wait to receive system message RELEASE
root.rpx will give up foreground by calling OSReleaseForeground

coreinit.OSReleaseForeground
  - syscall 0x2800 -> KiProcRelease

kernel.KiProcRelease
  0xFFF167B0
    0xFFF16054
      PhysAlloc
      KiInitAddrSpace
      log +Loader(%d,%d)
      KiRPLStartup
        KiRPLLoaderSetup
          KiLoadContext(__Loader_start)

loader.__LoaderStart
  - Setup heaps
  - LOADER_Init
  - Loader_ContinueStartProcess (syscall 0x5F)

kernel.Syscall 0x5F ContinueStartProces
  log -Loader(%d,%d)
  SetCoreToProcessId
  SetFG
    MCP ioctl 0x5C
  kernel.KiSetProcSchedMode
     (i guess in here the proc action callblack is triggered which leads to GAIN FOREGROUND message to happen)
  ProcessLaunch
     - Effectively sets up a new context and calls process entry point
     - mtsrr0
     - rfi


----------------------------

Load shared library for first time:
Load .text to 01000000
Load .data to 10000000, relocate .text against it
Load .rodata to shared data (0xF8000000)
Load a copy of .data to shared data, so games can copy it when they load a shared library

Load shared library for Nth time:
Copy the shared .data, must assert same address because of relocated .text

Loader:
Load all .rpx and .rpl .text sections
Load shared system library .data sections
Call coreinit entry point
Coreinit entry point will call __preinit_user if defined for .rpx
Allocate data sections for .rpx and non-shared .rpl (with OSDynLoad allocator)
Call entry point for everything


List of system shared libraries:
"coreinit", "tve", "nsysccr", "nsysnet", "uvc", "tcl", "nn_pdm", "dmae", "dc",
"vpadbase", "vpad", "avm", "gx2", "snd_core"

For shared libraries:
.text into 0x01000000
R/O data sections into 0xF8000000
compressed R/W data sections into 0xF8000000
section headers into 0xF8000000


__preinit_user is called  from coreinit->start
DONT FORGET DYNLOAD_ALLOCATOR for dynamic loaded .rpl data sections

*/


// gpLoaderShared = 0xFA000000

// gpSharedCodeHeapTracking = 0xFA000018
// TINYHEAP_Setup(gpSharedCodeHeapTracking,  0x830, 0x01000000, 0x007E0000)

// gpSharedReadHeapTracking = 0xFA000848
// TINYHEAP_Setup(gpSharedReadHeapTracking, 0x1030, 0xF8000000, 0x03000000)

// TINYHEAP_Setup(sgpTrackComp, 0x430, 0x10000000, 0x60000000)

#include <cstdint>
#include <libcpu/be2_struct.h>

namespace cafe::loader
{

static uint32_t gProcFlags = 0;
static uint32_t gEFE01000 = 0;
static uint32_t gEFE01008 = 0;
static uint32_t gEFE0100C = 0;
static uint32_t gEFE01010 = 0;
static uint32_t gEFE01014 = 0;
static uint32_t gEFE01018 = 0;
static uint32_t gDataAreaSize = 0;
static uint32_t gDataAreaAllocations = 0;

int32_t
TINYHEAP_Setup(virt_ptr<void> trackingHeap,
               uint32_t trackingSize,
               virt_ptr<void> heap,
               uint32_t heapSize)
{
   // Returns 0 or -520001
   return 0;
}

void
LOADER_Init(uint32_t r3, uint32_t num_codearea_heap_blocks, uint32_t maxCodeSize, uint32_t r6, virt_ptr<uint32_t> r7, virt_ptr<virt_ptr<void>> r8, virt_ptr<void> r9)
{
   gEFE01000 = r3;
   gEFE0100C = num_codearea_heap_blocks;

   if (maxCodeSize > 0x0E000000) {
      maxCodeSize = 0x0E000000; // 224mb
   }

   gEFE01008 = (num_codearea_heap_blocks * 16) + 0x30 + 0x3F;
   gEFE01010 = maxCodeSize - align_up(gEFE01008, 64);


   gEFE01014 = maxCodeSize;
   gEFE01018 = r6;
   *r7 = 0u;
   *r8 = nullptr;

   std::memset(r9.getRawPointer(), 0, 4 * 0xA);

   gDataAreaSize = 0u;
   gDataAreaAllocations = 0u;
   /*
   TINYHEAP_Setup(gEFE01004 = 0x10000000 - align_up(gEFE01008, 64),
                  gEFE01008,
                  0x10000000 - align_up(gEFE01008, 64) - gEFE01010,
                  gEFE01010);
   */
   if (0) {
      *r7 = 0x400u;
      *r8 = nullptr; // TODO: = something
   }

}

void
LiInitSharedForProcess(virt_ptr<void> unk)
{
   if (gProcFlags & 1) {

   }
}

/*

LiInitSharedForProcess


LiInitSharedForAll
- sLoadOneShared coreinit.rpl
- sLoadOneShared tve.rpl, nsysccr.rpl, nsysnet.rpl, uvc.rpl, tcl.rpl, nn_pdm.rpl, dmae.rpl,
  dc.rpl, vpadbase.rpl, vpad.rpl, avm.rpl, gx2.rpl, snd_core.rpl
- TINYHEAP_Alloc(gpSharedCodeHeapTracking) for tracking compression blocks for shared data? (TEMPORARY)
- TINYHEAP_Alloc(gpSharedReadHeapTracking) for compressed shared tracking data?
- TINYHEAP_Alloc(gpSharedCodeHeapTracking) for compressed initialisation data? (TEMPORARY)
- TINYHEAP_Alloc(gpSharedReadHeapTracking) for shared initialisation .data?

sLoadOneShared(rpl):
- Calls LiLoadRPLBasics
- Calls LiSetupOneRPL
- TINYHEAP_Alloc(gpSharedReadHeapTracking) for shared .rodata
- TINYHEAP_AllocAt(sgpTrackComp) for shared .bss (and space for .data presumably?)
- LiCacheLineCorrectAllocEx(0xEFE01004) Could not allocate space for shared import tracking in local heap

LiLoadRPLBasics(r6 = gpSharedCodeHeapTracking, r7 = gpSharedReadHeapTracking, r9 = isShared):
- r24 = gpSharedCodeHeapTracking
- r25 = gpSharedReadHeapTracking
- r27 = isShared
- LiCacheLineCorrectAllocEx(gpSharedCodeHeapTracking) = Could not allocate uncompressed text
- LiCacheLineCorrectAllocEx(gpSharedReadHeapTracking) = Failed {workarea, readheap) alloc
- LiCacheLineCorrectAllocEx(gpSharedReadHeapTracking) = Failed to allocate %d bytes for section addresses
- LiCacheLineCorrectAllocEx(gpSharedReadHeapTracking) = Could not allocate space for section headers in {shared readonly, readonly} heap
- LiCacheLineCorrectAllocEx(0xEFE01004) = Could not allocate space for module name
- LiCacheLineCorrectAllocEx(0xEFE01004) = Could not allocate space for CRCs
- LiCacheLineCorrectAllocEx(gpSharedReadHeapTracking) = Could not allocate space for fileinfo

LiSetupOneRPL(r5 = gpSharedCodeHeapTracking, r6 = gpSharedReadHeapTracking):
- r29 = gpSharedCodeHeapTracking
- LiCacheLineCorrectAllocEx(gpSharedCodeHeapTracking) - Allocation failed for compressed relocations from RPL code heap

*/

} // namespace cafe::loader

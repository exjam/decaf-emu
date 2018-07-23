#pragma optimize("", off)
#include "kernel_enum.h"
#include "kernel_memory.h"

#include <algorithm>
#include <common/align.h>
#include <common/decaf_assert.h>
#include <common/log.h>
#include <libcpu/address.h>
#include <libcpu/mem.h>
#include <libcpu/mmu.h>
#include <libcpu/pointer.h>
#include <vector>

namespace kernel
{

struct PhysicalRange
{
   cpu::PhysicalAddress start;
   cpu::PhysicalAddress end;

   uint32_t size() const
   {
      return static_cast<uint32_t>(end - start + 1);
   }
};

struct VirtualMap
{
   cpu::VirtualAddress start;
   cpu::VirtualAddress end;
   PhysicalRegion physicalRegion;
   bool mapped = false;

   uint32_t size() const
   {
      return static_cast<uint32_t>(end - start + 1);
   }
};

static cpu::PhysicalAddress operator "" _paddr(unsigned long long int x)
{
   return cpu::PhysicalAddress { static_cast<uint32_t>(x) };
}

static cpu::VirtualAddress operator "" _vaddr(unsigned long long int x)
{
   return cpu::VirtualAddress { static_cast<uint32_t>(x) };
}

static std::map<PhysicalRegion, PhysicalRange>
sPhysicalMemoryMap {
   // MEM1 - 32 MB
   { PhysicalRegion::MEM1,                         { 0x00000000_paddr , 0x01FFFFFF_paddr } },

   // HACK: LockedCache (not present on hardware) - 16 KB per core
   // Note we have to use the page size, 128kb, as that is the minimum we can map.
   { PhysicalRegion::LockedCache,                  { 0x02000000_paddr, 0x0201FFFF_paddr } },

   // MEM0 - 2.68 MB / 2,752 KB
   { PhysicalRegion::MEM0,                         { 0x08000000_paddr, 0x082DFFFF_paddr } },
   { PhysicalRegion::MEM0CafeKernel,               { 0x08000000_paddr, 0x080DFFFF_paddr } },
   // unused cafe kernel                             0x080E0000    --  0x0811FFFF
   { PhysicalRegion::MEM0IosKernel,                { 0x08120000_paddr, 0x081BFFFF_paddr } },
   { PhysicalRegion::MEM0IosMcp,                   { 0x081C0000_paddr, 0x0827FFFF_paddr } },
   { PhysicalRegion::MEM0IosCrypto,                { 0x08280000_paddr, 0x082BFFFF_paddr } },
   // IOS BSP unknown                                0x082C0000   --   0x082DFFFF

   // MMIO / Registers
   //  0x0C000000  --  0x0C??????
   //  0x0D000000  --  0x0D??????

   // MEM2 - 2 GB
   { PhysicalRegion::MEM2,                         { 0x10000000_paddr, 0x8FFFFFFF_paddr } },
   // IOS MCP unknown                                0x10000000   --   0x100FFFFF
   { PhysicalRegion::MEM2IosUsb,                   { 0x10100000_paddr, 0x106FFFFF_paddr } },
   { PhysicalRegion::MEM2IosFs,                    { 0x10700000_paddr, 0x11EFFFFF_paddr } },
   { PhysicalRegion::MEM2IosPad,                   { 0x11F00000_paddr, 0x122FFFFF_paddr } },
   { PhysicalRegion::MEM2IosNet,                   { 0x12300000_paddr, 0x128FFFFF_paddr } },
   { PhysicalRegion::MEM2IosAcp,                   { 0x12900000_paddr, 0x12BBFFFF_paddr } },
   { PhysicalRegion::MEM2IosNsec,                  { 0x12BC0000_paddr, 0x12EBFFFF_paddr } },
   { PhysicalRegion::MEM2IosNim,                   { 0x12EC0000_paddr, 0x1363FFFF_paddr } },
   { PhysicalRegion::MEM2IosFpd,                   { 0x13640000_paddr, 0x13A3FFFF_paddr } },
   { PhysicalRegion::MEM2IosTest,                  { 0x13A40000_paddr, 0x13BFFFFF_paddr } },
   { PhysicalRegion::MEM2IosAuxil,                 { 0x13C00000_paddr, 0x13CBFFFF_paddr } },
   { PhysicalRegion::MEM2IosBsp,                   { 0x13CC0000_paddr, 0x13D7FFFF_paddr } },
   // IOS MCP unknown                                0x13D80000   --   0x13DBFFFF
   { PhysicalRegion::MEM2ForegroundBucket,         { 0x14000000_paddr, 0x17FFFFFF_paddr } },
   { PhysicalRegion::MEM2SharedData,               { 0x18000000_paddr, 0x1AFFFFFF_paddr } },
   { PhysicalRegion::MEM2LoaderBounceBuffer,       { 0x1B000000_paddr, 0x1B7FFFFF_paddr } },
   { PhysicalRegion::MEM2CafeKernelWorkAreaHeap,   { 0x1B800000_paddr, 0x1B87FFFF_paddr } },
   // Unknown                                        0x1B880000   --   0x1B8FFFFF
   { PhysicalRegion::MEM2LoaderGlobals,            { 0x1B900000_paddr, 0x1B97FFFF_paddr } },
   // Unknown                                        0x1B980000   --   0x1BFFFFFF
   { PhysicalRegion::MEM2LoaderHeap,               { 0x1C000000_paddr, 0x1CFFFFFF_paddr } },
   { PhysicalRegion::MEM2IosSharedHeap,            { 0x1D000000_paddr, 0x1FAFFFFF_paddr } },
   { PhysicalRegion::MEM2IosNetIobuf,              { 0x1FB00000_paddr, 0x1FDFFFFF_paddr } },
   // IOS MCP unknown                                0x1FE00000   --   0x1FE3FFFF
   // IOS MCP unknown                                0x1FE40000   --   0x1FFFFFFF
   { PhysicalRegion::MEM2IosFsRamdisk,             { 0x20000000_paddr, 0x27FFFFFF_paddr } },
   { PhysicalRegion::MEM2HomeMenu,                 { 0x28000000_paddr, 0x2FFFFFFF_paddr } },
   { PhysicalRegion::MEM2Root,                     { 0x30000000_paddr, 0x31FFFFFF_paddr } },
   { PhysicalRegion::MEM2CafeOS,                   { 0x32000000_paddr, 0x327FFFFF_paddr } },
   // unknown? maybe reserved for CafeOS increase? 0x32800000 -- 0x32FFFFFF
   { PhysicalRegion::MEM2ErrorDisplay,             { 0x33000000_paddr, 0x33FFFFFF_paddr } },
   { PhysicalRegion::MEM2OverlayApp,               { 0x34000000_paddr, 0x4FFFFFFF_paddr } },
   { PhysicalRegion::MEM2MainApp,                  { 0x50000000_paddr, 0x8FFFFFFF_paddr } },
   { PhysicalRegion::MEM2DevKit,                   { 0x90000000_paddr, 0xCFFFFFFF_paddr } },

   // HACK: Tiling Apertures (not present on hardware)
   { PhysicalRegion::TilingApertures,              { 0xD0000000_paddr, 0xDFFFFFFF_paddr } },

   // SRAM1 - 32 KB
   //0xFFF00000   --   0xFFF07FFF = IOS MCP sram1
   { PhysicalRegion::SRAM1,                        { 0xFFF00000_paddr, 0xFFF07FFF_paddr } },
   { PhysicalRegion::SRAM1C2W,                     { 0xFFF00000_paddr, 0xFFF07FFF_paddr } },

   // SRAM0 - 64 KB
   //0xFFFF0000   --   0xFFFFFFFF = IOS kernel sram0
   { PhysicalRegion::SRAM0,                        { 0xFFFF0000_paddr, 0xFFFFFFFF_paddr } },
   { PhysicalRegion::SRAM0IosKernel,               { 0xFFFF0000_paddr, 0xFFFFFFFF_paddr } },
};

static std::map<VirtualRegion, VirtualMap>
sVirtualMemoryMap {
   { VirtualRegion::CafeOS,                  { 0x01000000_vaddr, 0x017FFFFF_vaddr, PhysicalRegion::MEM2CafeOS } },
   { VirtualRegion::MainAppCode,             { 0x02000000_vaddr, 0x0FFFFFFF_vaddr, PhysicalRegion::MEM2MainApp } },
   { VirtualRegion::MainAppData,             { 0x10000000_vaddr, 0x4FFFFFFF_vaddr, PhysicalRegion::MEM2MainApp } },
   { VirtualRegion::OverlayArena,            { 0x60000000_vaddr, 0x7BFFFFFF_vaddr, PhysicalRegion::MEM2OverlayApp } },
   { VirtualRegion::TilingApertures,         { 0x80000000_vaddr, 0x8FFFFFFF_vaddr, PhysicalRegion::TilingApertures } },
   { VirtualRegion::VirtualMapRange,         { 0xA0000000_vaddr, 0xDFFFFFFF_vaddr, PhysicalRegion::Invalid } },
   { VirtualRegion::ForegroundBucket,        { 0xE0000000_vaddr, 0xE3FFFFFF_vaddr, PhysicalRegion::MEM2ForegroundBucket } },
   { VirtualRegion::LoaderGlobals,           { 0xEFE00000_vaddr, 0xEFE7FFFF_vaddr, PhysicalRegion::MEM2LoaderGlobals } },
   { VirtualRegion::MEM1,                    { 0xF4000000_vaddr, 0xF5FFFFFF_vaddr, PhysicalRegion::MEM1 } },
   { VirtualRegion::LoaderBounceBuffer,      { 0xF6000000_vaddr, 0xF67FFFFF_vaddr, PhysicalRegion::MEM2LoaderBounceBuffer } },
   { VirtualRegion::SharedData,              { 0xF8000000_vaddr, 0xFAFFFFFF_vaddr, PhysicalRegion::MEM2SharedData } },
   { VirtualRegion::KernelWorkAreaHeap,      { 0xFF200000_vaddr, 0xFF27FFFF_vaddr, PhysicalRegion::MEM2CafeKernelWorkAreaHeap } },
   { VirtualRegion::LockedCache,             { 0xFFC00000_vaddr, 0xFFC1FFFF_vaddr, PhysicalRegion::LockedCache } },
   { VirtualRegion::KernelStatic,            { 0xFFE00000_vaddr, 0xFFEDFFFF_vaddr, PhysicalRegion::MEM0CafeKernel } },
};

/*
Virtual Map dumped from kernel:
                vaddr,       size,      paddr,      flags
MemoryMap < 0x1000000,   0x800000, 0x32000000, 0x2CE08002> // cafe os
MemoryMap < 0x1800000,    0x20000,          0, 0x28101200> // unknown
MemoryMap < 0x2000000,          0,          0, 0x2CF09400> // app code
MemoryMap <0x10000000,          0,          0, 0x28305800> // app data
MemoryMap <0xA0000000, 0x40000000,          0,     0x2000> // unk (wiiubru thinks mapping used by loader to load app code/data)
MemoryMap <0xE0000000,  0x4000000, 0x14000000, 0x28204004> // foreground bucket
MemoryMap <0xE8000000,  0x2000000, 0xD0000000, 0x78200004> // unk 32mb
MemoryMap <0xEFE00000,    0x80000, 0x1B900000, 0x28109010> // loader globals, also read / write by kernel
MemoryMap <0xF4000000,  0x2000000,          0, 0x28204004> // MEM1
MemoryMap <0xF6000000,   0x800000, 0x1B000000, 0x3CA08002> // loader bounce buffer, 8mb
MemoryMap <0xF8000000,  0x3000000, 0x18000000, 0x2CA08002> // shared data
MemoryMap <0xFB000000,   0x800000, 0x1C800000, 0x28200002> // unknown 8mb
MemoryMap <0xFC000000,    0xC0000,  0xC000000, 0x70100022> // registers
MemoryMap <0xFC0C0000,   0x120000,  0xC0C0000, 0x70100022> // registers
MemoryMap <0xFC1E0000,    0x20000,  0xC1E0000, 0x78100024> // registers
MemoryMap <0xFC200000,    0x80000,  0xC200000, 0x78100024> // registers
MemoryMap <0xFC280000,    0x20000,  0xC280000, 0x78100024> // registers
MemoryMap <0xFC2A0000,    0x20000,  0xC2A0000, 0x78100023> // write gather memory
                                    0xC2A0000 = write gather core 0
                                    0xC2C0000 = write gather core 0
                                    0xC2E0000 = write gather core 2
MemoryMap <0xFC300000,    0x20000,  0xC300000, 0x78100024> // registers
MemoryMap <0xFC320000,    0xE0000,  0xC320000, 0x70100022> // registers
MemoryMap <0xFD000000,   0x400000,  0xD000000, 0x70100022> // registers
MemoryMap <0xFE000000,   0x800000, 0x1C000000, 0x20200002> // unkown 8mb

MemoryMap <0xFF200000,    0x80000, 0x1B800000, 0x20100040> // Kernel "Work Area" heap
MemoryMap <0xFF280000,    0x80000, 0x1B880000, 0x20100040> // Kernel unknown

MemoryMap <0xFFC00000,    0x20000, 0xFFC00000,  0x8100004> // code gen area 0
MemoryMap <0xFFC40000,    0x20000, 0xFFC40000,  0x8100004> // code gen area 1
MemoryMap <0xFFC80000,    0x20000, 0xFFC80000,  0x810000C> // code gen area 2

MemoryMap <0xFFCE0000,    0x20000,          0, 0x50100004> // unknown

MemoryMap <0xFFE00000,    0x20000, 0xFFE00000, 0x20100040> // kernel data
MemoryMap <0xFFE40000,    0x20000, 0xFFE40000, 0x20100040> // kernel data
MemoryMap <0xFFE80000,    0x60000, 0xFFE80000, 0x20100040> // kernel data

MemoryMap <0xFFF60000,    0x20000, 0xFFE20000, 0x20100080> // unknown
MemoryMap <0xFFF80000,    0x20000, 0xFFE60000, 0x2C100040> // unknown
MemoryMap <0xFFFA0000,    0x20000, 0xFFE60000, 0x20100080> // unknown

MemoryMap <0xFFFC0000,    0x20000, 0x1BFE0000, 0x24100002> // unknown
MemoryMap <0xFFFE0000,    0x20000, 0x1BF80000, 0x28100102> // unknown
*/


static constexpr uint32_t MinCodeSize = 0x20000;
static constexpr uint32_t MaxCodeSize = 0xE000000;

static auto sUnknownReservedCodeBase = cpu::PhysicalAddress { 0 };
static auto sUnknownReservedCodeSize = uint32_t { 0 };

static auto sAvailPhysicalBase = cpu::PhysicalAddress { 0 };
static auto sAvailPhysicalSize = uint32_t { 0 };

static auto sCodeGenPhysicalBase = cpu::PhysicalAddress { 0 };
static auto sCodeGenPhysicalSize = uint32_t { 0 };

static bool
map(VirtualMap &map)
{
   auto &physicalRegion = sPhysicalMemoryMap[map.physicalRegion];
   auto physSize = static_cast<uint32_t>(physicalRegion.end - physicalRegion.start + 1);
   auto size = static_cast<uint32_t>(map.end - map.start + 1);
   decaf_check(physSize == size);

   if (!map.mapped) {
      if (!cpu::allocateVirtualAddress(map.start, size)) {
         gLog->error("Unexpected failure allocating virtual address 0x{:08X} - 0x{:08X}",
                     map.start.getAddress(), map.end.getAddress());
         return false;
      }

      if (!cpu::mapMemory(map.start, physicalRegion.start, size, cpu::MapPermission::ReadWrite)) {
         gLog->error("Unexpected failure allocating mapping virtual address 0x{:08X} to physical address 0x{:08X}",
                     map.start.getAddress(), physicalRegion.start.getAddress());
         return false;
      }

      map.mapped = true;
   }

   return true;
}

static void
unmap(VirtualMap &map)
{
   if (map.mapped) {
      auto size = static_cast<uint32_t>(map.end - map.start + 1);
      if (!cpu::unmapMemory(map.start, size)) {
         gLog->error("Unexpected failure unmapping virtual address 0x{:08X} - 0x{:08X}",
                     map.start.getAddress(), map.end.getAddress());
      }

      if (!cpu::freeVirtualAddress(map.start, size)) {
         gLog->error("Unexpected failure freeing virtual address 0x{:08X} - 0x{:08X}",
                     map.start.getAddress(), map.end.getAddress());
      }

      map.mapped = false;
   }
}

void
initialiseVirtualMemory()
{
   map(sVirtualMemoryMap[VirtualRegion::CafeOS]);
   map(sVirtualMemoryMap[VirtualRegion::ForegroundBucket]);
   map(sVirtualMemoryMap[VirtualRegion::MEM1]);
   map(sVirtualMemoryMap[VirtualRegion::LoaderBounceBuffer]);
   map(sVirtualMemoryMap[VirtualRegion::LoaderGlobals]);
   map(sVirtualMemoryMap[VirtualRegion::SharedData]);
   map(sVirtualMemoryMap[VirtualRegion::LockedCache]);
   map(sVirtualMemoryMap[VirtualRegion::KernelWorkAreaHeap]);
   map(sVirtualMemoryMap[VirtualRegion::KernelStatic]);
}

void
freeVirtualMemory()
{
   unmap(sVirtualMemoryMap[VirtualRegion::CafeOS]);
   unmap(sVirtualMemoryMap[VirtualRegion::ForegroundBucket]);
   unmap(sVirtualMemoryMap[VirtualRegion::MEM1]);
   unmap(sVirtualMemoryMap[VirtualRegion::LoaderBounceBuffer]);
   unmap(sVirtualMemoryMap[VirtualRegion::LoaderGlobals]);
   unmap(sVirtualMemoryMap[VirtualRegion::SharedData]);
   unmap(sVirtualMemoryMap[VirtualRegion::LockedCache]);
   unmap(sVirtualMemoryMap[VirtualRegion::KernelWorkAreaHeap]);
   unmap(sVirtualMemoryMap[VirtualRegion::KernelStatic]);

   freeOverlayArena();
   freeTilingApertures();
   freeAppMemory();
}

cpu::VirtualAddressRange
initialiseOverlayArena()
{
   auto &region = sVirtualMemoryMap[VirtualRegion::OverlayArena];
   map(region);
   return { region.start, region.end };
}

void
freeOverlayArena()
{
   unmap(sVirtualMemoryMap[VirtualRegion::OverlayArena]);
}

cpu::VirtualAddressRange
initialiseTilingApertures()
{
   auto &region = sVirtualMemoryMap[VirtualRegion::TilingApertures];
   map(region);
   return { region.start, region.end };
}

void
freeTilingApertures()
{
   unmap(sVirtualMemoryMap[VirtualRegion::TilingApertures]);
}

bool
initialiseAppMemory(uint32_t codeSize,
                    uint32_t codeGenSize,
                    uint32_t availSize)
{
   const auto &physicalAppHeap = sPhysicalMemoryMap[PhysicalRegion::MEM2MainApp];
   auto reservedSize = uint32_t { 0 };

   codeSize = std::max(std::min(codeSize, MaxCodeSize), MinCodeSize);
   codeSize = align_up(codeSize, cpu::PageSize);
   codeGenSize = align_up(codeGenSize, cpu::PageSize);
   availSize = align_up(availSize, cpu::PageSize);
   reservedSize = align_up(0x60000, cpu::PageSize);

   // Calculate physical mapping, the order going backwards from end is:
   // PhysicalAppHeap.end -> code -> code gen -> unknown -> avail
   auto physicalDataStart = physicalAppHeap.start;
   auto physicalDataEnd = physicalAppHeap.end - codeSize - codeGenSize - reservedSize - availSize;
   auto physicalCodeStart = physicalAppHeap.end - codeSize + 1;
   auto physicalCodeEnd = physicalAppHeap.end;
   auto dataSize = physicalDataEnd - physicalDataStart;

   sCodeGenPhysicalSize = codeGenSize;
   sCodeGenPhysicalBase = physicalCodeStart - sCodeGenPhysicalSize;

   sUnknownReservedCodeSize = reservedSize;
   sUnknownReservedCodeBase = sCodeGenPhysicalBase - sUnknownReservedCodeSize;

   sAvailPhysicalSize = availSize;
   sAvailPhysicalBase = sUnknownReservedCodeBase - sAvailPhysicalSize;

   // Adjust virtual mapping
   auto &virtualCodeMap = sVirtualMemoryMap[VirtualRegion::MainAppCode];
   auto &virtualDataMap = sVirtualMemoryMap[VirtualRegion::MainAppData];
   virtualCodeMap.end = virtualCodeMap.start + codeSize - 1;
   virtualDataMap.end = virtualDataMap.start + dataSize - 1;

   if (!cpu::allocateVirtualAddress(virtualCodeMap.start, codeSize)) {
      gLog->error("Unexpected failure allocating code virtual address 0x{:08X} - 0x{:08X}",
                  virtualCodeMap.start.getAddress(), virtualCodeMap.end.getAddress());
      return false;
   }

   if (!cpu::allocateVirtualAddress(virtualDataMap.start, dataSize)) {
      gLog->error("Unexpected failure allocating data virtual address 0x{:08X} - 0x{:08X}",
                  virtualDataMap.start.getAddress(), virtualDataMap.end.getAddress());
      return false;
   }

   if (!cpu::mapMemory(virtualCodeMap.start,
                       physicalCodeStart,
                       codeSize,
                       cpu::MapPermission::ReadWrite)) {
      gLog->error("Unexpected failure mapping data virtual address 0x{:08X} - 0x{:08X} to physical address 0x{:08X}",
                  virtualCodeMap.start.getAddress(), virtualCodeMap.end.getAddress(), physicalCodeStart.getAddress());
      return false;
   }

   virtualCodeMap.mapped = true;

   if (!cpu::mapMemory(virtualDataMap.start,
                       physicalDataStart,
                       dataSize,
                       cpu::MapPermission::ReadWrite)) {
      gLog->error("Unexpected failure mapping data virtual address 0x{:08X} - 0x{:08X} to physical address 0x{:08X}",
                  virtualDataMap.start.getAddress(), virtualDataMap.end.getAddress(), physicalDataStart.getAddress());
      return false;
   }

   virtualDataMap.mapped = true;
   return true;
}

void
freeAppMemory()
{
   auto &appHeapCode = sVirtualMemoryMap[VirtualRegion::MainAppCode];
   auto &appHeapData = sVirtualMemoryMap[VirtualRegion::MainAppData];

   if (appHeapCode.mapped) {
      cpu::unmapMemory(appHeapCode.start, appHeapCode.size());
      cpu::freeVirtualAddress(appHeapCode.start, appHeapCode.size());
      appHeapCode.mapped = false;
   }

   if (appHeapData.mapped) {
      cpu::unmapMemory(appHeapData.start, appHeapData.size());
      cpu::freeVirtualAddress(appHeapData.start, appHeapData.size());
      appHeapData.mapped = false;
   }
}

cpu::VirtualAddressRange
getVirtualRange(VirtualRegion region)
{
   return { sVirtualMemoryMap[region].start, sVirtualMemoryMap[region].end };
}

cpu::PhysicalAddressRange
getPhysicalRange(PhysicalRegion region)
{
   return { sPhysicalMemoryMap[region].start, sPhysicalMemoryMap[region].end };
}

cpu::PhysicalAddressRange
getAvailPhysicalRange()
{
   return { sAvailPhysicalBase, sAvailPhysicalSize };
}

cpu::PhysicalAddressRange
getDataPhysicalRange()
{
   return {
      sPhysicalMemoryMap[PhysicalRegion::MEM2MainApp].start,
      sVirtualMemoryMap[VirtualRegion::MainAppData].size()
   };
}

static bool
inVirtualMapRange(cpu::VirtualAddress address,
                  uint32_t size)
{
   auto range = kernel::getVirtualRange(kernel::VirtualRegion::VirtualMapRange);
   return (range.start <= address && address + size <= range.start + range.size);
}

cpu::VirtualAddress
allocVirtAddr(cpu::VirtualAddress address,
              uint32_t size,
              uint32_t alignment)
{
   if (alignment < cpu::PageSize) {
      alignment = cpu::PageSize;
   }

   if (address) {
      if (!inVirtualMapRange(address, size)) {
         return cpu::VirtualAddress { 0u };
      }

      address = align_up(address, alignment);
      size = align_up(size, alignment);
   } else {
      auto virtualMapRange = kernel::getVirtualRange(kernel::VirtualRegion::VirtualMapRange);
      auto range = cpu::findFreeVirtualAddressInRange(virtualMapRange, size, alignment);
      address = range.start;
      size = range.size;
   }

   if (cpu::allocateVirtualAddress(address, size)) {
      return address;
   } else {
      return cpu::VirtualAddress { 0u };
   }
}

bool
freeVirtAddr(cpu::VirtualAddress address,
             uint32_t size)
{
   if (!inVirtualMapRange(address, size)) {
      return false;
   }

   return cpu::freeVirtualAddress(address, size);
}

bool
mapMemory(cpu::VirtualAddress virtAddr,
          cpu::PhysicalAddress physAddr,
          uint32_t size,
          MapPermission permission)
{
   if (!inVirtualMapRange(virtAddr, size)) {
      return false;
   }

   auto cpuPermission = cpu::MapPermission { };

   if (permission == MapPermission::ReadOnly) {
      cpuPermission = cpu::MapPermission::ReadOnly;
   } else if (permission == MapPermission::ReadWrite) {
      cpuPermission = cpu::MapPermission::ReadWrite;
   } else {
      gLog->error("Unexpected mapMemory permission: {}", permission);
      return false;
   }

   return cpu::mapMemory(virtAddr, physAddr, size, cpuPermission);
}

bool
unmapMemory(cpu::VirtualAddress virtAddr,
            uint32_t size)
{
   if (!inVirtualMapRange(virtAddr, size)) {
      return false;
   }

   return cpu::unmapMemory(virtAddr, size);
}

VirtualMemoryType
queryVirtAddr(cpu::VirtualAddress address)
{
   if (!inVirtualMapRange(address, 0)) {
      return VirtualMemoryType::Invalid;
   }

   auto cpuType = cpu::queryVirtualAddress(address);

   switch (cpuType) {
   case cpu::VirtualMemoryType::MappedReadOnly:
      return VirtualMemoryType::MappedReadOnly;
   case cpu::VirtualMemoryType::MappedReadWrite:
      return VirtualMemoryType::MappedReadWrite;
   case cpu::VirtualMemoryType::Free:
      return VirtualMemoryType::Free;
   case cpu::VirtualMemoryType::Allocated:
      return VirtualMemoryType::Allocated;
   default:
      return VirtualMemoryType::Invalid;
   }
}

} // namespace kernel

#include "cafe_kernel_mmu.h"

#include <cstdint>
#include <common/log.h>
#include <map>
#include <libcpu/mmu.h>

namespace cafe::kernel
{

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

constexpr cpu::PhysicalAddress operator "" _paddr(unsigned long long int x)
{
   return cpu::PhysicalAddress { static_cast<uint32_t>(x) };
}

constexpr cpu::VirtualAddress operator "" _vaddr(unsigned long long int x)
{
   return cpu::VirtualAddress { static_cast<uint32_t>(x) };
}

static std::map<PhysicalRegion, phys_addr_range>
sPhysicalMemoryMap {
   // MEM1 - 32 MB
   { PhysicalRegion::MEM1,                   { 0x00000000_paddr , 0x01FFFFFF_paddr } },

   // LockedCache (not present on hardware) - 16 KB per core
   // Note we have to use the page size, 128kb, as that is the minimum we can map.
   { PhysicalRegion::LockedCache,            { 0x02000000_paddr, 0x0201FFFF_paddr } },

   // MEM0 - 2.68 MB / 2,752 KB
   { PhysicalRegion::MEM0,                   { 0x08000000_paddr, 0x082DFFFF_paddr } },
   { PhysicalRegion::MEM0CafeKernel,         { 0x08000000_paddr, 0x080DFFFF_paddr } },
   // unused cafe kernel                       0x080E0000    --  0x0811FFFF
   { PhysicalRegion::MEM0IosKernel,          { 0x08120000_paddr, 0x081BFFFF_paddr } },
   { PhysicalRegion::MEM0IosMcp,             { 0x081C0000_paddr, 0x0827FFFF_paddr } },
   { PhysicalRegion::MEM0IosCrypto,          { 0x08280000_paddr, 0x082BFFFF_paddr } },
   // IOS BSP unknown                          0x082C0000   --   0x082DFFFF

   // MMIO / Registers
   //  0x0C000000  --  0x0C??????
   //  0x0D000000  --  0x0D??????

   // MEM2 - 2 GB
   { PhysicalRegion::MEM2,                   { 0x10000000_paddr, 0x8FFFFFFF_paddr } },
   // IOS MCP unknown                          0x10000000   --   0x100FFFFF
   { PhysicalRegion::MEM2IosUsb,             { 0x10100000_paddr, 0x106FFFFF_paddr } },
   { PhysicalRegion::MEM2IosFs,              { 0x10700000_paddr, 0x11EFFFFF_paddr } },
   { PhysicalRegion::MEM2IosPad,             { 0x11F00000_paddr, 0x122FFFFF_paddr } },
   { PhysicalRegion::MEM2IosNet,             { 0x12300000_paddr, 0x128FFFFF_paddr } },
   { PhysicalRegion::MEM2IosAcp,             { 0x12900000_paddr, 0x12BBFFFF_paddr } },
   { PhysicalRegion::MEM2IosNsec,            { 0x12BC0000_paddr, 0x12EBFFFF_paddr } },
   { PhysicalRegion::MEM2IosNim,             { 0x12EC0000_paddr, 0x1363FFFF_paddr } },
   { PhysicalRegion::MEM2IosFpd,             { 0x13640000_paddr, 0x13A3FFFF_paddr } },
   { PhysicalRegion::MEM2IosTest,            { 0x13A40000_paddr, 0x13BFFFFF_paddr } },
   { PhysicalRegion::MEM2IosAuxil,           { 0x13C00000_paddr, 0x13CBFFFF_paddr } },
   { PhysicalRegion::MEM2IosBsp,             { 0x13CC0000_paddr, 0x13D7FFFF_paddr } },
   // IOS MCP unknown                          0x13D80000   --   0x13DBFFFF
   { PhysicalRegion::MEM2ForegroundBucket,   { 0x14000000_paddr, 0x17FFFFFF_paddr } },
   { PhysicalRegion::MEM2SharedData,         { 0x18000000_paddr, 0x1AFFFFFF_paddr } },
   { PhysicalRegion::MEM2CafeKernelHeap,     { 0x1B000000_paddr, 0x1BFFFFFF_paddr } },
   { PhysicalRegion::MEM2LoaderHeap,         { 0x1C000000_paddr, 0x1CFFFFFF_paddr } },
   { PhysicalRegion::MEM2IosSharedHeap,      { 0x1D000000_paddr, 0x1FAFFFFF_paddr } },
   { PhysicalRegion::MEM2IosNetIobuf,        { 0x1FB00000_paddr, 0x1FDFFFFF_paddr } },
   // IOS MCP unknown                          0x1FE00000   --   0x1FE3FFFF
   // IOS MCP unknown                          0x1FE40000   --   0x1FFFFFFF
   { PhysicalRegion::MEM2IosFsRamdisk,       { 0x20000000_paddr, 0x27FFFFFF_paddr } },
   { PhysicalRegion::MEM2HomeMenu,           { 0x28000000_paddr, 0x2FFFFFFF_paddr } },
   { PhysicalRegion::MEM2Root,               { 0x30000000_paddr, 0x31FFFFFF_paddr } },
   { PhysicalRegion::MEM2CafeOS,             { 0x32000000_paddr, 0x327FFFFF_paddr } },
   // unknown? maybe reserved for CafeOS increase? 0x32800000 -- 0x32FFFFFF
   { PhysicalRegion::MEM2ErrorDisplay,       { 0x33000000_paddr, 0x33FFFFFF_paddr } },
   { PhysicalRegion::MEM2OverlayApp,         { 0x34000000_paddr, 0x4FFFFFFF_paddr } },
   { PhysicalRegion::MEM2MainApp,            { 0x50000000_paddr, 0x8FFFFFFF_paddr } },
   { PhysicalRegion::MEM2DevKit,             { 0x90000000_paddr, 0xCFFFFFFF_paddr } },

   // HACK: Tiling Apertures (not present on hardware)
   { PhysicalRegion::TilingApertures,        { 0xD0000000_paddr, 0xDFFFFFFF_paddr } },

   // SRAM1 - 32 KB
   //0xFFF00000   --   0xFFF07FFF = IOS MCP sram1
   { PhysicalRegion::SRAM1,                  { 0xFFF00000_paddr, 0xFFF07FFF_paddr } },
   { PhysicalRegion::SRAM1C2W,               { 0xFFF00000_paddr, 0xFFF07FFF_paddr } },

   // SRAM0 - 64 KB
   //0xFFFF0000   --   0xFFFFFFFF = IOS kernel sram0
   { PhysicalRegion::SRAM0,                  { 0xFFFF0000_paddr, 0xFFFFFFFF_paddr } },
   { PhysicalRegion::SRAM0IosKernel,         { 0xFFFF0000_paddr, 0xFFFFFFFF_paddr } },
};

phys_addr_range
getPhysicalAddressRange(PhysicalRegion region)
{
   return sPhysicalMemoryMap.at(region);
}

phys_addr_range
getAvailPhysAddrRange()
{
   // TODO: getAvailPhysAddrRange
   return { phys_addr { 0 }, 0 };
}

phys_addr_range
getDataPhysAddrRange()
{
   // TODO: getDataPhysAddrRange
   return { phys_addr { 0 }, 0 };
}

// Memory mapped for kernel
static std::map<VirtualRegion, VirtualMap>
sKernelVirtualMemoryMap {
   { VirtualRegion::KernelHeap,              { 0xFF200000_vaddr, 0xFF2FFFFF_vaddr, PhysicalRegion::MEM2CafeKernelHeap } },
   { VirtualRegion::KernelStatic,            { 0xFFE00000_vaddr, 0xFFEDFFFF_vaddr, PhysicalRegion::MEM0CafeKernel } },
};

// Virtual memory which is mapped for ALL processes.
static std::map<VirtualRegion, VirtualMap>
sGlobalVirtualMemoryMap {
   { VirtualRegion::CafeOS,                  { 0x01000000_vaddr, 0x017FFFFF_vaddr, PhysicalRegion::MEM2CafeOS } },
   { VirtualRegion::SharedData,              { 0xF8000000_vaddr, 0xFAFFFFFF_vaddr, PhysicalRegion::MEM2SharedData } },
};

// Virtual memory mapped when a process is in the foreground
static std::map<VirtualRegion, VirtualMap>
sForegroundAppMemoryMap {
   { VirtualRegion::MEM1,                    { 0xF4000000_vaddr, 0xF5FFFFFF_vaddr, PhysicalRegion::MEM1 } },
   { VirtualRegion::ForegroundBucket,        { 0xE0000000_vaddr, 0xE3FFFFFF_vaddr, PhysicalRegion::MEM2ForegroundBucket } },
   { VirtualRegion::LockedCache,             { 0xFFC00000_vaddr, 0xFFC1FFFF_vaddr, PhysicalRegion::LockedCache } }, // Core 0, 1, 2
   { VirtualRegion::TilingApertures,         { 0x80000000_vaddr, 0x8FFFFFFF_vaddr, PhysicalRegion::TilingApertures } },
};

// Virtual memory mapped when a process is in the background
static std::map<VirtualRegion, VirtualMap>
sBackgroundAppMemoryMap {
   { VirtualRegion::LockedCache,             { 0xFFC00000_vaddr, 0xFFC1FFFF_vaddr, PhysicalRegion::LockedCache } }, // Only Core 2
};

// Virtual memory mapped only for overlay process.
static std::map<VirtualRegion, VirtualMap>
sOverlayAppMemoryMap {
   { VirtualRegion::OverlayAppCode,          {          0_vaddr,          0_vaddr, PhysicalRegion::MEM2OverlayApp } },
   { VirtualRegion::OverlayAppData,          {          0_vaddr,          0_vaddr, PhysicalRegion::MEM2OverlayApp } },
};

constexpr auto OverlayAppCodeBase = 0x02000000_vaddr;
constexpr auto OverlayAppDataBase = 0x10000000_vaddr;
constexpr auto OverlayAppDataEnd = 0x4FFFFFFF_vaddr;

// Virtual memory mapped only for main process.
static std::map<VirtualRegion, VirtualMap>
sMainApplicationMemoryMap {
   { VirtualRegion::MainAppCode,             {          0_vaddr,          0_vaddr, PhysicalRegion::MEM2MainApp } },
   { VirtualRegion::MainAppData,             {          0_vaddr,          0_vaddr, PhysicalRegion::MEM2MainApp } },
};

constexpr auto MainAppCodeBase = 0x02000000_vaddr;
constexpr auto MainAppDataBase = 0x10000000_vaddr;
constexpr auto MainAppDataEnd = 0x4FFFFFFF_vaddr;

static bool
map(VirtualMap &map)
{
   auto &physicalRange = sPhysicalMemoryMap[map.physicalRegion];
   auto size = static_cast<uint32_t>(map.end - map.start + 1);
   decaf_check(physicalRange.size == size);

   if (!map.mapped) {
      if (!cpu::allocateVirtualAddress(map.start, size)) {
         gLog->error("Unexpected failure allocating virtual address 0x{:08X} - 0x{:08X}",
                     map.start.getAddress(), map.end.getAddress());
         return false;
      }

      if (!cpu::mapMemory(map.start, physicalRange.start, size, cpu::MapPermission::ReadWrite)) {
         gLog->error("Unexpected failure allocating mapping virtual address 0x{:08X} to physical address 0x{:08X}",
                     map.start.getAddress(), physicalRange.start.getAddress());
         return false;
      }

      map.mapped = true;
   }

   return true;
}

static bool
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

virt_addr_range
getKernelVirtualAddressRange(VirtualRegion region)
{
   auto mapping = sKernelVirtualMemoryMap.at(region);
   return virt_addr_range { mapping.start, mapping.end };
}

virt_addr_range
getMapVirtAddrRange()
{
   // TODO: getMapVirtAddrRange
   return { virt_addr { 0 }, 0 };
}

virt_addr_range
getForegroundBucket()
{
   auto mapping = sForegroundAppMemoryMap.at(VirtualRegion::ForegroundBucket);
   return virt_addr_range { mapping.start, mapping.end };
}

bool
mapKernelVirtualMemory()
{
   auto success = true;
   for (auto keyValue : sKernelVirtualMemoryMap) {
      success = success && map(keyValue.second);
   }

   return success;
}

bool
mapGlobalVirtualMemory()
{
   auto success = true;
   for (auto keyValue : sGlobalVirtualMemoryMap) {
      success = success && map(keyValue.second);
   }

   return success;
}

virt_addr
allocVirtAddr(virt_addr address,
              uint32_t size,
              uint32_t alignment)
{
   // TODO: allocVirtAddr
   return virt_addr { 0 };
}

BOOL
freeVirtAddr(virt_addr address,
             uint32_t size)
{
   // TODO: freeVirtAddr
   return FALSE;
}

int
queryVirtAddr(virt_addr address)
{
   // TODO: queryVirtAddr
   return 0;
}

BOOL
mapMemory(virt_addr virtAddress,
          phys_addr physAddress,
          uint32_t size,
          int permission)
{
   // TODO: mapMemory
   return FALSE;
}

BOOL
unmapMemory(virt_addr virtAddress,
            uint32_t size)
{
   // TODO: unmapMemory
   return FALSE;
}

phys_addr
virtualToPhysical(virt_addr va)
{
   phys_addr pa;
   decaf_check(cpu::virtualToPhysicalAddress(va, pa));
   return pa;
}

virt_addr
physicalToVirtual(phys_addr pa)
{
   // TODO: physicalToVirtual
   return virt_addr { 0 };
}

namespace internal
{

void
initialiseMmu()
{
   // Note: mapKernelVirtualMemory will have already been called by this point.
   mapGlobalVirtualMemory();
}

} // namespace internal

/*
Dumped from kernel:
                vaddr,       size,      paddr,      flags
MemoryMap < 0x1000000,   0x800000, 0x32000000, 0x2CE08002> // cafe os
MemoryMap < 0x1800000,    0x20000,          0, 0x28101200> // unknown
MemoryMap < 0x2000000,          0,          0, 0x2CF09400> // app code
MemoryMap <0x10000000,          0,          0, 0x28305800> // app data
MemoryMap <0xA0000000, 0x40000000,          0,     0x2000> // unk (wiiubru thinks mapping used by loader to load app code/data)
MemoryMap <0xE0000000,  0x4000000, 0x14000000, 0x28204004> // foreground bucket
MemoryMap <0xE8000000,  0x2000000, 0xD0000000, 0x78200004> // unk 32mb
MemoryMap <0xEFE00000,    0x80000, 0x1B900000, 0x28109010> // kernel <-> loader IPC data
MemoryMap <0xF4000000,  0x2000000,          0, 0x28204004> // MEM1
MemoryMap <0xF6000000,   0x800000, 0x1B000000, 0x3CA08002> // loader bounce buffer, 8mb
MemoryMap <0xF8000000,  0x3000000, 0x18000000, 0x2CA08002> // loader shared read heap
MemoryMap <0xFB000000,   0x800000, 0x1C800000, 0x28200002> // unknown 8mb
MemoryMap <0xFC000000,    0xC0000,  0xC000000, 0x70100022> // registers
MemoryMap <0xFC0C0000,   0x120000,  0xC0C0000, 0x70100022> // registers
MemoryMap <0xFC1E0000,    0x20000,  0xC1E0000, 0x78100024> // registers
MemoryMap <0xFC200000,    0x80000,  0xC200000, 0x78100024> // registers
MemoryMap <0xFC280000,    0x20000,  0xC280000, 0x78100024> // registers
MemoryMap <0xFC2A0000,    0x20000,  0xC2A0000, 0x78100023> // registers
MemoryMap <0xFC300000,    0x20000,  0xC300000, 0x78100024> // registers
MemoryMap <0xFC320000,    0xE0000,  0xC320000, 0x70100022> // registers
MemoryMap <0xFD000000,   0x400000,  0xD000000, 0x70100022> // registers
MemoryMap <0xFE000000,   0x800000, 0x1C000000, 0x20200002> // unkown 8mb

MemoryMap <0xFF200000,    0x80000, 0x1B800000, 0x20100040> // KERNEL HEAP ?
MemoryMap <0xFF280000,    0x80000, 0x1B880000, 0x20100040> // KERNEL HEAP ?

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

} // namespace cafe::kernel

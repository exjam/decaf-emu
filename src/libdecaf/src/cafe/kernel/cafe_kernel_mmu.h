#pragma once
#include <libcpu/be2_struct.h>

namespace cafe::kernel
{

enum class PhysicalRegion
{
   Invalid = -1,
   MEM1 = 0,
   LockedCache,
   MEM0,
   MEM0CafeKernel,
   MEM0IosKernel,
   MEM0IosMcp,
   MEM0IosCrypto,
   MEM2,
   MEM2IosUsb,
   MEM2IosFs,
   MEM2IosPad,
   MEM2IosNet,
   MEM2IosAcp,
   MEM2IosNsec,
   MEM2IosNim,
   MEM2IosFpd,
   MEM2IosTest,
   MEM2IosAuxil,
   MEM2IosBsp,
   MEM2ForegroundBucket,
   MEM2SharedData,
   MEM2CafeKernelHeap,
   MEM2LoaderHeap,
   MEM2IosSharedHeap,
   MEM2IosNetIobuf,
   MEM2IosFsRamdisk,
   MEM2HomeMenu,
   MEM2Root,
   MEM2CafeOS,
   MEM2ErrorDisplay,
   MEM2OverlayApp,
   MEM2MainApp,
   MEM2DevKit,
   TilingApertures,
   SRAM1,
   SRAM1C2W,
   SRAM0,
   SRAM0IosKernel,
};

enum class VirtualRegion
{
   Invalid = -1,
   CafeOS = 0,
   MainAppCode,
   MainAppData,
   OverlayAppCode,
   OverlayAppData,
   TilingApertures,
   VirtualMapRange,
   ForegroundBucket,
   MEM1,
   SharedData,
   LockedCache,
   KernelStatic,
   KernelHeap,
};

phys_addr_range
getPhysicalAddressRange(PhysicalRegion region);

phys_addr_range
getAvailPhysAddrRange();

phys_addr_range
getDataPhysAddrRange();

virt_addr_range
getKernelVirtualAddressRange(VirtualRegion region);

virt_addr_range
getMapVirtAddrRange();

virt_addr_range
getForegroundBucket();

bool
mapKernelVirtualMemory();

bool
mapGlobalVirtualMemory();

virt_addr
allocVirtAddr(virt_addr address,
              uint32_t size,
              uint32_t alignment);

BOOL
freeVirtAddr(virt_addr address,
             uint32_t size);

int
queryVirtAddr(virt_addr address);

BOOL
mapMemory(virt_addr virtAddress,
          phys_addr physAddress,
          uint32_t size,
          int permission);

BOOL
unmapMemory(virt_addr virtAddress,
            uint32_t size);

phys_addr
virtualToPhysical(virt_addr pa);

virt_addr
physicalToVirtual(phys_addr pa);

template<typename Type>
virt_ptr<Type>
physicalToVirtual(phys_ptr<Type> ptr)
{
   return virt_cast<Type *>(physicalToVirtual(phys_cast<phys_addr>(ptr)));
}

template<typename Type>
phys_ptr<Type>
virtualToPhysical(virt_ptr<Type> ptr)
{
   return phys_cast<Type *>(virtualToPhysical(virt_cast<virt_addr>(ptr)));
}

template<typename Type>
virt_ptr<Type>
physicalToVirtual(be2_phys_ptr<Type> ptr)
{
   return virt_cast<Type *>(physicalToVirtual(phys_cast<phys_addr>(ptr)));
}

template<typename Type>
phys_ptr<Type>
virtualToPhysical(be2_virt_ptr<Type> ptr)
{
   return phys_cast<Type *>(virtualToPhysical(virt_cast<virt_addr>(ptr)));
}

namespace internal
{

void
initialiseMmu();

} // namespace internal

} // namespace cafe::kernel

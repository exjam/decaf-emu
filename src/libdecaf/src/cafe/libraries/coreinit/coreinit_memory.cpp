#include "coreinit.h"
#include "coreinit_memory.h"
#include "coreinit_internal_idlock.h"
#include "cafe/kernel/cafe_kernel_mmu.h"

#include <atomic>
#include <cstring>

namespace cafe::coreinit
{

struct StaticMemoryData
{
   internal::IdLock boundsLock;
   be2_val<virt_addr> foregroundBaseAddress;
   be2_val<virt_addr> boundsMem1Addr;
   be2_val<virt_addr> boundsMem2Addr;
   be2_val<uint32_t> boundsMem1Size;
   be2_val<uint32_t> boundsMem2Size;
};

static virt_ptr<StaticMemoryData>
getMemoryData()
{
   return nullptr;
}

virt_ptr<void>
OSBlockMove(virt_ptr<void> dst,
            virt_ptr<const void> src,
            uint32_t size,
            BOOL flush)
{
   memmove(dst, src, size);
   return dst;
}

virt_ptr<void>
OSBlockSet(virt_ptr<void> dst,
           int val,
           uint32_t size)
{
   memset(dst, val, size);
   return dst;
}

/**
 * Get the foreground memory bucket address and size.
 *
 * \return
 * Returns TRUE if the current process is in the foreground.
 */
BOOL
OSGetForegroundBucket(virt_ptr<virt_addr> addr,
                      virt_ptr<uint32_t> size)
{
   auto range = kernel::getForegroundBucket();

   if (addr) {
      *addr = range.start;
   }

   if (size) {
      *size = range.size;
   }

   return range.start && range.size;
}

enum ForegroundArea
{
   Application = 0,
   Unknown1 = 1,
   Unknown2 = 1,
   Unknown3 = 1,
   Unknown4 = 1,
   Unknown5 = 1,
   Unknown6 = 1,
   Unknown7 = 1,
};

static virt_ptr<void>
getFgMemPtr(ForegroundArea area)
{
   struct FgMemInfo
   {
      ForegroundArea area;
      uint32_t offset;
      uint32_t size;
   };

   static constexpr std::array<FgMemInfo, 8> fgMemInfo = {
      FgMemInfo { ForegroundArea::Application,           0, 0x2800000 },
      FgMemInfo { ForegroundArea::Unknown7,      0x2800000,  0x400000 },
      FgMemInfo { ForegroundArea::Unknown1,      0x2C00000,  0x900000 },
      FgMemInfo { ForegroundArea::Unknown2,      0x3500000,  0x3C0000 },
      FgMemInfo { ForegroundArea::Unknown3,      0x38C0000,  0x1C0000 },
      FgMemInfo { ForegroundArea::Unknown4,      0x3A80000,  0x3C0000 },
      FgMemInfo { ForegroundArea::Unknown5,      0x3E40000,  0x1BF000 },
      FgMemInfo { ForegroundArea::Unknown6,      0x3FFF000,    0x1000 },
   };

   auto memoryData = getMemoryData();

   for (auto &memInfo : fgMemInfo) {
      if (memInfo.area == area) {
         return virt_cast<void *>(memoryData->foregroundBaseAddress + memInfo.offset);
      }
   }

   return nullptr;
}

BOOL
OSGetForegroundBucketFreeArea(virt_ptr<virt_addr> addr,
                              virt_ptr<uint32_t> size)
{
   auto memoryData = getMemoryData();

   if (addr) {
      *addr = virt_cast<virt_addr>(getFgMemPtr(ForegroundArea::Application));
   }

   if (size) {
      *size = 40 * 1024 * 1024;
   }

   return memoryData->foregroundBaseAddress != virt_addr { 0 };
}

int
OSGetMemBound(OSMemoryType type,
              virt_ptr<virt_addr> addr,
              virt_ptr<uint32_t> size)
{
   auto memoryData = getMemoryData();

   if (addr) {
      *addr = 0;
   }

   if (size) {
      *size = 0;
   }

   internal::acquireIdLockWithCoreId(memoryData->boundsLock);
   switch (type) {
   case OSMemoryType::MEM1:
      if (addr) {
         *addr = memoryData->boundsMem1Addr;
      }

      if (size) {
         *size = memoryData->boundsMem1Size;
      }
      break;
   case OSMemoryType::MEM2:
      if (addr) {
         *addr = memoryData->boundsMem2Addr;
      }

      if (size) {
         *size = memoryData->boundsMem2Size;
      }
      break;
   default:
      internal::releaseIdLockWithCoreId(memoryData->boundsLock);
      return -1;
   }

   internal::releaseIdLockWithCoreId(memoryData->boundsLock);
   return 0;
}

void
OSGetAvailPhysAddrRange(virt_ptr<phys_addr> start,
                        virt_ptr<uint32_t> size)
{
   auto range = kernel::getAvailPhysAddrRange();

   if (start) {
      *start = range.start;
   }

   if (size) {
      *size = range.size;
   }
}

void
OSGetDataPhysAddrRange(virt_ptr<phys_addr> start,
                       virt_ptr<uint32_t> size)
{
   auto range = kernel::getDataPhysAddrRange();

   if (start) {
      *start = range.start;
   }

   if (size) {
      *size = range.size;
   }
}

void
OSGetMapVirtAddrRange(virt_ptr<virt_addr> start,
                      virt_ptr<uint32_t> size)
{
   auto range = kernel::getMapVirtAddrRange();

   if (start) {
      *start = range.start;
   }

   if (size) {
      *size = range.size;
   }
}

virt_addr
OSAllocVirtAddr(virt_addr address,
                uint32_t size,
                uint32_t alignment)
{
   return kernel::allocVirtAddr(address, size, alignment);
}

BOOL
OSFreeVirtAddr(virt_addr address,
               uint32_t size)
{
   return kernel::freeVirtAddr(address, size);
}

int
OSQueryVirtAddr(virt_addr address)
{
   return kernel::queryVirtAddr(address);
}

BOOL
OSMapMemory(virt_addr virtAddress,
            phys_addr physAddress,
            uint32_t size,
            int permission)
{
   return kernel::mapMemory(virtAddress, physAddress, size, permission);
}

BOOL
OSUnmapMemory(virt_addr virtAddress,
              uint32_t size)
{
   return kernel::unmapMemory(virtAddress, size);
}

/**
 * Translates a virtual (effective) address to a physical address.
 */
phys_addr
OSEffectiveToPhysical(virt_addr address)
{
   // TODO: OSEffectiveToPhysical
   return phys_addr { 0 };
}

/**
 * Translates a physical address to a virtual (effective) address.
 */
virt_addr
OSPhysicalToEffectiveCached(phys_addr address)
{
   // TODO: OSPhysicalToEffectiveCached
   return virt_addr { 0 };
}

/**
 * Translates a physical address to a virtual (effective) address.
 */
virt_addr
OSPhysicalToEffectiveUncached(phys_addr address)
{
   // TODO: OSPhysicalToEffectiveUncached
   return virt_addr { 0 };
}


/**
 * memcpy for virtual memory.
 */
virt_ptr<void>
memcpy(virt_ptr<void> dst,
       virt_ptr<const void> src,
       uint32_t size)
{
   std::memcpy(dst.getRawPointer(), src.getRawPointer(), size);
   return dst;
}


/**
 * memmove for virtual memory.
 */
virt_ptr<void>
memmove(virt_ptr<void> dst,
        virt_ptr<const void> src,
        uint32_t size)
{
   std::memmove(dst.getRawPointer(), src.getRawPointer(), size);
   return dst;
}


/**
 * memset for virtual memory.
 */
virt_ptr<void>
memset(virt_ptr<void> dst,
       int value,
       uint32_t size)
{
   std::memset(dst.getRawPointer(), value, size);
   return dst;
}

void
Library::registerMemoryFunctions()
{
   RegisterFunctionExport(OSBlockMove);
   RegisterFunctionExport(OSBlockSet);
   RegisterFunctionExport(OSGetMemBound);
   RegisterFunctionExport(OSGetForegroundBucket);
   RegisterFunctionExport(OSGetForegroundBucketFreeArea);
   RegisterFunctionExport(OSGetAvailPhysAddrRange);
   RegisterFunctionExport(OSGetDataPhysAddrRange);
   RegisterFunctionExport(OSGetMapVirtAddrRange);
   RegisterFunctionExport(OSAllocVirtAddr);
   RegisterFunctionExport(OSFreeVirtAddr);
   RegisterFunctionExport(OSQueryVirtAddr);
   RegisterFunctionExport(OSMapMemory);
   RegisterFunctionExport(OSUnmapMemory);
   RegisterFunctionExport(OSEffectiveToPhysical);
   RegisterFunctionExport(OSPhysicalToEffectiveCached);
   RegisterFunctionExport(OSPhysicalToEffectiveUncached);
   RegisterFunctionExport(memcpy);
   RegisterFunctionExport(memmove);
   RegisterFunctionExport(memset);
}

} // namespace cafe::coreinit

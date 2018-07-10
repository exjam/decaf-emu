#include "coreinit.h"
#include "coreinit_memory.h"
#include "coreinit_internal_idlock.h"
#include "kernel/kernel_memory.h"

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
sMemoryData = nullptr;

enum ForegroundAreaId
{
   Application = 0,
   TransitionAudioBuffer = 1,
   SavedFrameUnk2 = 2,
   SavedFrameUnk3 = 3,
   SavedFrameUnk4 = 4,
   SavedFrameUnk5 = 5,
   Unknown6 = 6,
   CopyArea = 7,
};

struct ForegroundArea
{
   ForegroundAreaId id;
   uint32_t offset;
   uint32_t size;
};

constexpr std::array<ForegroundArea, 8> ForegroundAreas {
   ForegroundArea { ForegroundAreaId::Application,                   0, 0x2800000 },
   ForegroundArea { ForegroundAreaId::CopyArea,              0x2800000,  0x400000 },
   ForegroundArea { ForegroundAreaId::TransitionAudioBuffer, 0x2C00000,  0x900000 },
   ForegroundArea { ForegroundAreaId::SavedFrameUnk2,        0x3500000,  0x3C0000 },
   ForegroundArea { ForegroundAreaId::SavedFrameUnk3,        0x38C0000,  0x1C0000 },
   ForegroundArea { ForegroundAreaId::SavedFrameUnk4,        0x3A80000,  0x3C0000 },
   ForegroundArea { ForegroundAreaId::SavedFrameUnk5,        0x3E40000,  0x1BF000 },
   ForegroundArea { ForegroundAreaId::Unknown6,              0x3FFF000,    0x1000 },
};

static virt_ptr<void>
getForegroundAreaPointer(ForegroundAreaId id)
{
   for (auto &area : ForegroundAreas) {
      if (area.id == id) {
         return virt_cast<void *>(sMemoryData->foregroundBaseAddress + area.offset);
      }
   }

   return nullptr;
}

static uint32_t
getForegroundAreaSize(ForegroundAreaId id)
{
   for (auto &area : ForegroundAreas) {
      if (area.id == id) {
         return area.size;
      }
   }

   return 0;
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
   auto range = kernel::getVirtualRange(kernel::VirtualRegion::ForegroundBucket);

   if (addr) {
      *addr = range.start;
   }

   if (size) {
      *size = range.size;
   }

   return range.start && range.size;
}


/**
 * Get the area of the foreground bucket which the application can use.
 *
 * \return
 * Returns TRUE if the current process is in the foreground.
 */
BOOL
OSGetForegroundBucketFreeArea(virt_ptr<virt_addr> addr,
                              virt_ptr<uint32_t> size)
{
   if (addr) {
      *addr = virt_cast<virt_addr>(getForegroundAreaPointer(ForegroundAreaId::Application));
   }

   if (size) {
      *size = getForegroundAreaSize(ForegroundAreaId::Application);
   }

   return !!sMemoryData->foregroundBaseAddress;
}

int32_t
OSGetMemBound(OSMemoryType type,
              virt_ptr<virt_addr> addr,
              virt_ptr<uint32_t> size)
{
   if (addr) {
      *addr = virt_addr { 0u };
   }

   if (size) {
      *size = 0u;
   }

   internal::acquireIdLockWithCoreId(sMemoryData->boundsLock);
   switch (type) {
   case OSMemoryType::MEM1:
      if (addr) {
         *addr = sMemoryData->boundsMem1Addr;
      }

      if (size) {
         *size = sMemoryData->boundsMem1Size;
      }
      break;
   case OSMemoryType::MEM2:
      if (addr) {
         *addr = sMemoryData->boundsMem2Addr;
      }

      if (size) {
         *size = sMemoryData->boundsMem2Size;
      }
      break;
   default:
      internal::releaseIdLockWithCoreId(sMemoryData->boundsLock);
      return -1;
   }

   internal::releaseIdLockWithCoreId(sMemoryData->boundsLock);
   return 0;
}

void
OSGetAvailPhysAddrRange(virt_ptr<phys_addr> start,
                        virt_ptr<uint32_t> size)
{
   auto range = ::kernel::getAvailPhysicalRange();

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
   auto range = kernel::getDataPhysicalRange();

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
   auto range = ::kernel::getVirtualRange(::kernel::VirtualRegion::VirtualMapRange);

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
   return ::kernel::allocVirtAddr(address, size, alignment);
}

BOOL
OSFreeVirtAddr(virt_addr address,
               uint32_t size)
{
   return ::kernel::freeVirtAddr(address, size) ? TRUE : FALSE;
}

int32_t
OSQueryVirtAddr(virt_addr address)
{
   return static_cast<int32_t>(::kernel::queryVirtAddr(address));
}

BOOL
OSMapMemory(virt_addr virtAddress,
            phys_addr physAddress,
            uint32_t size,
            int permission)
{
   return ::kernel::mapMemory(virtAddress,
                              physAddress,
                              size,
                              static_cast<::kernel::MapPermission>(permission))
      ? TRUE : FALSE;
}

BOOL
OSUnmapMemory(virt_addr virtAddress,
              uint32_t size)
{
   return kernel::unmapMemory(virtAddress, size) ? TRUE : FALSE;
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
Library::registerMemorySymbols()
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

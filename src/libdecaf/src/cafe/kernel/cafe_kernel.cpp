#include "cafe_kernel.h"
#include "cafe_kernel_context.h"
#include "cafe_kernel_exception.h"
#include "cafe_kernel_ipc.h"
#include "cafe_kernel_ipckdriver.h"
#include "cafe_kernel_heap.h"
#include "cafe_kernel_mmu.h"

#include <atomic>
#include <common/frameallocator.h>
#include <libcpu/cpu.h>

namespace cafe::kernel
{

static std::atomic<bool>
sRunning = false;

static FrameAllocator
sStaticAllocator;

/*
PerCoreData
+0xB4 = RAMPID

*/

namespace internal
{

static void
core1EntryPoint()
{
   initialiseMmu();
   initialiseHeap();
   initialiseIpc();
   // ?????
   // Do stuff...


   while (sRunning.load()) {
      cpu::this_core::waitForInterrupt();
   }
}

static void
core0EntryPoint()
{
   while (sRunning.load()) {
      cpu::this_core::waitForInterrupt();
   }
}

static void
core2EntryPoint()
{
   while (sRunning.load()) {
      cpu::this_core::waitForInterrupt();
   }
}

static void
coreEntryPoint(cpu::Core *core)
{
   // Initialise contexts for this core
   initialiseCoreContext(core);
   initialiseExceptionContext(core);

   // Run the cores!
   switch (core->id) {
   case 0:
      core0EntryPoint();
      break;
   case 1:
      core1EntryPoint();
      break;
   case 2:
      core2EntryPoint();
      break;
   }
}

virt_ptr<void>
allocStatic(std::size_t size,
            std::size_t alignment)
{
   auto buffer = sStaticAllocator.allocate(size, alignment);
   std::memset(buffer, 0, size);
   return virt_cast<void *>(cpu::translate(buffer));
}

static void
initialiseStaticMemory()
{
   auto range = getKernelVirtualAddressRange(VirtualRegion::KernelStatic);
   sStaticAllocator = FrameAllocator {
      virt_cast<uint8_t *>(range.start).getRawPointer(),
      range.size
   };

   initialiseStaticContextData();
   initialiseStaticExceptionData();
   initialiseStaticIpcData();
}

} // namespace internal

void
start()
{
   // Map the kernel's virtual memory so we can initialise static memory
   mapKernelVirtualMemory();
   internal::initialiseStaticMemory();

   // TODO: cpu::initialiseMemory must be called before ios::start()
   internal::initialiseExceptionHandlers();
   cpu::setCoreEntrypointHandler(&internal::coreEntryPoint);
   cpu::start();
}

} // namespace cafe::kernel

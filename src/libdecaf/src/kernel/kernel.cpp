#include "debugger/debugger.h"
#include "decaf_events.h"
#include "decaf_config.h"
#include "filesystem/filesystem.h"
#include "kernel.h"
#include "kernel_hle.h"
#include "kernel_loader.h"
#include "kernel_memory.h"
#include "kernel_filesystem.h"
#include "ios/ios.h"
#include "cafe/libraries/gx2/gx2_event.h"
#include "modules/sci/sci_cafe_settings.h"
#include "modules/sci/sci_caffeine_settings.h"
#include "modules/sci/sci_parental_account_settings_uc.h"
#include "modules/sci/sci_parental_settings.h"
#include "modules/sci/sci_spot_pass_settings.h"

#include "cafe/libraries/cafe_hle.h"
#include "cafe/cafe_ppc_interface_invoke.h"
#include "cafe/loader/cafe_loader_globals.h"
#include "cafe/kernel/cafe_kernel_loader.h"
#include "cafe/kernel/cafe_kernel_exception.h"
#include "cafe/kernel/cafe_kernel_heap.h"

#include <common/decaf_assert.h>
#include <common/platform_dir.h>
#include <common/platform_fiber.h>
#include <common/platform_thread.h>
#include <common/teenyheap.h>
#include <fmt/format.h>
#include <libcpu/trace.h>
#include <libcpu/mem.h>
#include <pugixml.hpp>

namespace kernel
{

namespace functions
{
void
kcTraceHandler(const std::string& str)
{
   traceLogSyscall(str);
   gLog->debug(str);
}
}

enum class FaultReason : uint32_t {
   Unknown,
   Segfault,
   IllInst
};

static void
cpuEntrypoint(cpu::Core *core);

static void
cpuBranchTraceHandler(cpu::Core *core, uint32_t target);

static std::atomic_bool
sRunning { true };

static std::string
sExecutableName;

static TeenyHeap *
sSystemHeap = nullptr;

static loader::LoadedModule *
sUserModule;

static int
sExitCode = 0;

static FaultReason
sFaultReason = FaultReason::Unknown;

static uint32_t
sSegfaultAddress = 0;

static decaf::GameInfo
sGameInfo;

static std::array<const char *, 14>
sSharedLibraries = {
   "coreinit", "tve", "nsysccr", "nsysnet", "uvc", "tcl", "nn_pdm", "dmae",
   "dc", "vpadbase", "vpad", "avm", "gx2", "snd_core"
};


void
setExecutableFilename(const std::string& name)
{
   sExecutableName = name;
}

void
initialise()
{
   cafe::hle::initialiseLibraries();

   // Initialise memory
   initialiseVirtualMemory();
   cafe::kernel::internal::initialiseStaticDataHeap();

   // Initialise static data
   cafe::kernel::internal::initialiseStaticContextData();
   cafe::kernel::internal::initialiseStaticExceptionData();

   // Initialise HLE modules
   // initialiseHleModules();

   // Setup cpu
   cpu::setCoreEntrypointHandler(&cpuEntrypoint);
   cafe::kernel::internal::initialiseExceptionHandlers();

   if (decaf::config::log::branch_trace) {
      cpu::setBranchTraceHandler(&cpuBranchTraceHandler);
   }

   auto bounds = kernel::getVirtualRange(kernel::VirtualRegion::CafeOS);
   sSystemHeap = new TeenyHeap { virt_cast<void *>(bounds.start).getRawPointer(), bounds.size };
}

void
shutdown()
{
   ios::join();
}

TeenyHeap *
getSystemHeap()
{
   return sSystemHeap;
}

static void
cpuBranchTraceHandler(cpu::Core *core, uint32_t target)
{
   /*auto symNamePtr = loader::findSymbolNameForAddress(target);
   if (!symNamePtr) {
      return;
   }

   gLog->debug("CPU branched to: {}", *symNamePtr);*/
}

static void
coreWfiLoop(cpu::Core *core)
{
   // Set up the default expected state for the nia/cia of idle threads.
   //  This must be kept in sync with reschedule which sets them to this
   //  for debugging purposes.
   core->nia = 0xFFFFFFFF;
   core->cia = 0xFFFFFFFF;

   while (sRunning.load()) {
      cpu::this_core::waitForInterrupt();
   }
}

static void
core1EntryPoint(cpu::Core *core)
{
   /*
   Load coreinit.rpl .text & .data
   Load shared libraries .text
   Load rpx .text -> run __preinit_user
   Load shared libraries .data, load rpx .data
   */
   if (!loadGameInfo(sGameInfo)) {
      gLog->warn("Could not load game info.");
   } else {
      gLog->info("Loaded game info: '{}' - {} v{}", sGameInfo.meta.shortnames[decaf::Language::English], sGameInfo.meta.product_code, sGameInfo.meta.title_version);
   }

   auto rpx = sGameInfo.cos.argstr;

   if (rpx.empty()) {
      rpx = sExecutableName;
   }

   if (rpx.empty()) {
      gLog->error("Could not find game executable to load.");
      return;
   }

   // Set up the application memory with max_codesize
   initialiseAppMemory(sGameInfo.cos.max_codesize,
                       sGameInfo.cos.codegen_size,
                       sGameInfo.cos.avail_size);

   if (sGameInfo.cos.num_codearea_heap_blocks == 0) {
      sGameInfo.cos.num_codearea_heap_blocks = 256;
   }

   if (sGameInfo.cos.num_workarea_heap_blocks == 0) {
      sGameInfo.cos.num_workarea_heap_blocks = 512;
   }

   cafe::loader::setLoadRpxName(rpx);
   cafe::kernel::internal::KiRPLStartup(
      cafe::kernel::UniqueProcessId::Kernel,
      cafe::kernel::UniqueProcessId::Game,
      cafe::kernel::ProcessFlags::get(0).debugLevel(cafe::kernel::DebugLevel::Verbose),
      sGameInfo.cos.num_codearea_heap_blocks,
      sGameInfo.cos.max_codesize,
      getVirtualRange(VirtualRegion::MainAppData).size + 1,
      0);

   // Run coreinit entry point
   using EntryPointFn = virt_func_ptr<void(void)>;
   auto coreinitEntry = virt_func_cast<EntryPointFn>(cafe::loader::getKernelIpcStorage()->startInfo.coreinit->entryPoint);

   cafe::invoke(cpu::this_core::state(),
                coreinitEntry);

#if 0
   // Load the default shared libraries, but NOT run their entry points
   auto coreinitModule = loader::loadRPL("coreinit");
   for (auto &name : sSharedLibraries) {
      auto sharedModule = loader::loadRPL(name);

      if (!sharedModule) {
         gLog->warn("Could not load shared library {}", name);
      }
   }

   // Load the application
   auto appModule = loader::loadRPL(rpx);

   if (!appModule) {
      gLog->error("Could not load {}", rpx);
      return;
   }

   gLog->debug("Succesfully loaded {}", rpx);
   sUserModule = appModule;

   // We need to set some default stuff up...
   core->gqr[2].value = 0x40004;
   core->gqr[3].value = 0x50005;
   core->gqr[4].value = 0x60006;
   core->gqr[5].value = 0x70007;

   // Setup coreinit threads
   coreinit::internal::startAlarmCallbackThreads();
   coreinit::internal::startAppIoThreads();
   coreinit::internal::startDefaultCoreThreads();
   coreinit::internal::startDeallocatorThreads();

   // Initialise CafeOS
   coreinit::IPCDriverInit();
   coreinit::IPCDriverOpen();
   coreinit::internal::mcpInit();
   coreinit::internal::loadSharedData();

   // Notify frontend that game has loaded
   decaf::event::onGameLoaded(sGameInfo);

   // Start the entry thread!
   auto gameThreadEntry = coreinitModule->findFuncExport<uint32_t, uint32_t, void*>("GameThreadEntry");
   auto thread = coreinit::OSGetDefaultThread(1);
   thread->entryPoint = gameThreadEntry;
   coreinit::OSResumeThread(thread);
#endif
   // Trip an interrupt to force game thread to run
   cpu::interrupt(1, cpu::GENERIC_INTERRUPT);
   coreWfiLoop(core);
}

static void
core0EntryPoint(cpu::Core *core)
{
   coreWfiLoop(core);
   gLog->error("Core0 exit");
}

static void
core2EntryPoint(cpu::Core *core)
{
   coreWfiLoop(core);
   gLog->error("Core2 exit");
}

void
cpuEntrypoint(cpu::Core *core)
{
   cafe::kernel::internal::initialiseCoreContext(core);
   cafe::kernel::internal::initialiseExceptionContext(core);

   // Run the cores!
   switch (core->id) {
   case 0:
      core0EntryPoint(core);
      break;
   case 1:
      core1EntryPoint(core);
      break;
   case 2:
      core2EntryPoint(core);
      break;
   };
}

loader::LoadedModule *
getUserModule()
{
   return nullptr;
}

loader::LoadedModule *
getTLSModule(uint32_t index)
{
   return nullptr;
}

void
exitProcess(int code)
{
   sExitCode = code;
   sRunning = false;
   cpu::halt();
   cafe::kernel::switchContext(nullptr);
}

bool
hasExited()
{
   return !sRunning;
}

int
getExitCode()
{
   return sExitCode;
}

const decaf::GameInfo &
getGameInfo()
{
   return sGameInfo;
}

} // namespace kernel

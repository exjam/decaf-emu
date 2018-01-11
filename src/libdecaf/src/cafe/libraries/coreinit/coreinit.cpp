#include "coreinit.h"

namespace cafe::coreinit
{

virt_ptr<LibraryStaticData>
Library::getStaticData()
{
   return nullptr;
}

void
Library::registerExports()
{
   registerAllocatorExports();
   registerAtomicExports();
   registerMemlistFunctions();
   registerSystemInfoExports();
   registerTimeExports();
}

void
Library::CafeInit()
{
   // Init stuff:
   // Exception handlers
   // Alarms
   // Threads
   // UGQR
   // IPC Driver
}

void
Library::libraryEntryPoint()
{
   // TODO: Initialise static memory!

   // TODO: Initialise all of coreinit!

   // TODO: OSDynload shit
   //   syscall to loader which finishes loading
   //   Call preinit user
   //   If no preinit, set base heap handles
   //   run entry points of all .rpl files
   //   return .rpx entry point

   // TODO: Run the returned .rpx entry point
}

} // namespace cafe::coreinit
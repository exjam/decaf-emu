#include "cafe_kernel.h"

namespace cafe::kernel
{

/*
PerCoreData
+0xB4 = RAMPID

*/
void
KiInitAddrSpace()
{
}

void
startKernel()
{
   // Initialise memory map
   KiInitAddrSpace();

   // ?????
   // Do stuff...


}

} // namespace cafe::kernel

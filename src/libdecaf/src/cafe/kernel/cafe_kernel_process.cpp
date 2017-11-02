#include "cafe_kernel_process.h"

namespace cafe::kernel
{

RamProcessId
getRampidFromUpid(UniqueProcessId id)
{
   return RamProcessId::Invalid;
}


RamProcessId
getRampid()
{
   return RamProcessId::Invalid;
}

KernelProcessId
getKernelProcessId()
{
   return KernelProcessId::Invalid;
}

TitleId
getTitleId()
{
   return 0;
}

void
switchToProcess(UniqueProcessId id)
{

}

} // namespace cafe::kernel

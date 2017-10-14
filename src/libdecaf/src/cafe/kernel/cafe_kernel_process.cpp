#include "cafe_kernel_process.h"

namespace cafe::kernel
{

void
switchToProcess(UniqueProcessId id)
{

}

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

} // namespace cafe::kernel

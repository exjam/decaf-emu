#include "ios_kernel_process.h"
#include "ios_kernel_thread.h"
#include <cstring>

namespace ios::kernel
{

Error
IOS_GetCurrentProcessId()
{
   auto thread = internal::getCurrentThread();
   if (!thread) {
      return Error::Invalid;
   }

   return static_cast<Error>(thread->pid.value());
}

Error
IOS_GetProcessName(ProcessId process,
                   char *buffer)
{
   const char *name = nullptr;

   switch (process) {
   case ProcessId::KERNEL:
      name = "IOS-KERNEL";
      break;
   case ProcessId::MCP:
      name = "IOS-MCP";
      break;
   case ProcessId::BSP:
      name = "IOS-BSP";
      break;
   case ProcessId::CRYPTO:
      name = "IOS-CRYPTO";
      break;
   case ProcessId::USB:
      name = "IOS-USB";
      break;
   case ProcessId::FS:
      name = "IOS-FS";
      break;
   case ProcessId::PAD:
      name = "IOS-PAD";
      break;
   case ProcessId::NET:
      name = "IOS-NET";
      break;
   case ProcessId::ACP:
      name = "IOS-ACP";
      break;
   case ProcessId::NSEC:
      name = "IOS-NSEC";
      break;
   case ProcessId::AUXIL:
      name = "IOS-AUXIL";
      break;
   case ProcessId::NIM:
      name = "IOS-NIM";
      break;
   case ProcessId::FPD:
      name = "IOS-FPD";
      break;
   case ProcessId::IOSTEST:
      name = "IOS-TEST";
      break;
   case ProcessId::COSKERNEL:
      name = "COS-KERNEL";
      break;
   case ProcessId::COSROOT:
      name = "COS-ROOT";
      break;
   case ProcessId::COS02:
      name = "COS-02";
      break;
   case ProcessId::COS03:
      name = "COS-03";
      break;
   case ProcessId::COSOVERLAY:
      name = "COS-OVERLAY";
      break;
   case ProcessId::COSHBM:
      name = "COS-HBM";
      break;
   case ProcessId::COSERROR:
      name = "COS-ERROR";
      break;
   case ProcessId::COSMASTER:
      name = "COS-MASTER";
      break;
   default:
      return Error::Invalid;
   }

   strcpy(buffer, name);
   return Error::OK;
}

namespace internal
{

ProcessId
getCurrentProcessId()
{
   auto thread = getCurrentThread();
   if (!thread) {
      return ProcessId::KERNEL;
   }

   return thread->pid;
}

} // namespace internal

} // namespace ios::kernel

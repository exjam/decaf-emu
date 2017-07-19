#include "ios_process.h"
#include <cstring>

namespace ios
{

Result<IOSError>
getProcessName(IOSProcessID process,
               char *buffer)
{
   const char *name = nullptr;

   switch (process) {
   case IOSProcessID::KERNEL:
      name = "IOS-KERNEL";
      break;
   case IOSProcessID::MCP:
      name = "IOS-MCP";
      break;
   case IOSProcessID::BSP:
      name = "IOS-BSP";
      break;
   case IOSProcessID::CRYPTO:
      name = "IOS-CRYPTO";
      break;
   case IOSProcessID::USB:
      name = "IOS-USB";
      break;
   case IOSProcessID::FS:
      name = "IOS-FS";
      break;
   case IOSProcessID::PAD:
      name = "IOS-PAD";
      break;
   case IOSProcessID::NET:
      name = "IOS-NET";
      break;
   case IOSProcessID::ACP:
      name = "IOS-ACP";
      break;
   case IOSProcessID::NSEC:
      name = "IOS-NSEC";
      break;
   case IOSProcessID::AUXIL:
      name = "IOS-AUXIL";
      break;
   case IOSProcessID::NIM:
      name = "IOS-NIM";
      break;
   case IOSProcessID::FPD:
      name = "IOS-FPD";
      break;
   case IOSProcessID::IOSTEST:
      name = "IOS-TEST";
      break;
   case IOSProcessID::COSKERNEL:
      name = "COS-KERNEL";
      break;
   case IOSProcessID::COSROOT:
      name = "COS-ROOT";
      break;
   case IOSProcessID::COS02:
      name = "COS-02";
      break;
   case IOSProcessID::COS03:
      name = "COS-03";
      break;
   case IOSProcessID::COSOVERLAY:
      name = "COS-OVERLAY";
      break;
   case IOSProcessID::COSHBM:
      name = "COS-HBM";
      break;
   case IOSProcessID::COSERROR:
      name = "COS-ERROR";
      break;
   case IOSProcessID::COSMASTER:
      name = "COS-MASTER";
      break;
   default:
      return IOSError::Invalid;
   }

   strcpy(buffer, name);
   return IOSError::OK;
}

} // namespace ios

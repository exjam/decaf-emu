#include "cafe_kernel_process.h"

namespace cafe::kernel
{

RamProcessId
getRampidFromUpid(UniqueProcessId id)
{
   switch (id) {
   case UniqueProcessId::Kernel:
      return RamProcessId::Kernel;
   case UniqueProcessId::Root:
      return RamProcessId::Root;
   case UniqueProcessId::HomeMenu:
      return RamProcessId::MainApplication;
   case UniqueProcessId::TV:
      return RamProcessId::OverlayApp;
   case UniqueProcessId::EManual:
      return RamProcessId::OverlayApp;
   case UniqueProcessId::OverlayMenu:
      return RamProcessId::OverlayMenu;
   case UniqueProcessId::ErrorDisplay:
      return RamProcessId::ErrorDisplay;
   case UniqueProcessId::MiniMiiverse:
      return RamProcessId::OverlayApp;
   case UniqueProcessId::InternetBrowser:
      return RamProcessId::OverlayApp;
   case UniqueProcessId::Miiverse:
      return RamProcessId::OverlayApp;
   case UniqueProcessId::EShop:
      return RamProcessId::OverlayApp;
   case UniqueProcessId::FLV:
      return RamProcessId::OverlayApp;
   case UniqueProcessId::DownloadManager:
      return RamProcessId::OverlayApp;
   case UniqueProcessId::Game:
      return RamProcessId::MainApplication;
   default:
      decaf_abort(fmt::format("Unknown UniqueProcessId {}", static_cast<uint32_t>(id)));
   }
}

} // namespace cafe::kernel

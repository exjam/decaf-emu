#include "coreinit.h"
#include "coreinit_driver.h"

namespace cafe::coreinit
{

struct StaticDriverData
{
   be2_virt_ptr<OSDriver> registeredDrivers;
};

static virt_ptr<StaticDriverData>
getDriverData()
{
   return Library::getStaticData()->driverData;
}

/*
OSDriver_Register(-1, 40, ptr, 0x0) FS
OSDriver_Register(0, 2000, ptr, 0xCAFE0005) Button
OSDriver_Register(0, 10, ptr, 0xCAFE0004) CACHE
OSDriver_Register(0, 100000, ptr, 0x64) TEST
OSDriver_Register(0, 900, ptr, 0x14) ACPLoad
OSDriver_Register(0, 800, ptr, 0x9) Input
OSDriver_Register(0, 1000, ptr, 0xA) Clipboard
OSDriver_Register(0, 30, ptr, 0x1E) OSSetting
OSDriver_Register(0, -1, ptr, 0xCAFE0002) MEM
OSDriver_Register(0, 20, ptr, 0xCAFE0001) IPC
OSDriver_Register(rpl_entry.r3, 910, ptr, 0x0) ACP
OSDriver_Register(rpl_entry.r3, 150, ptr, 0x6) AVM
OSDriver_Register(rpl_entry.r3, 550, ptr, 0x0) CAM
OSDriver_Register(rpl_entry.r3, 100, ptr, 0x7) DC
OSDriver_Register(rpl_entry.r3, 250, ptr, 0) GX2
OSDriver_Register(rpl_entry.r3, 550, ptr, 0x10000814) ccr_mic
OSDriver_Register(rpl_entry.r3, 50, ptr, 0) NETWORK
OSDriver_Register(rpl_entry.r3, 50, ptr, 0) NETWORK
OSDriver_Register(rpl_entry.r3, 50, ptr, 0) NETWORK
OSDriver_Register(rpl_entry.r3, 500, ptr, 0xC) HPAD
OSDriver_Register(rpl_entry.r3, 50, ptr, 0) NETWORK2
OSDriver_Register(rpl_entry.r3, 950, ptr, 0) TEMP
OSDriver_Register(rpl_entry.r3, 50, ptr, 0xB) CCR
OSDriver_Register(rpl_entry.r3, 450, ptr, 0) HID
OSDriver_Register(rpl_entry.r3, 50, ptr, 0) NETWORK
OSDriver_Register(rpl_entry.r3, 160, ptr, 0) UHS
OSDriver_Register(rpl_entry.r3, 300, ptr, 0) NSYSUVD
OSDriver_Register(rpl_entry.r3, 550, ptr, 0) NTAG
OSDriver_Register(rpl_entry.r3, 150, ptr, 2) WBC
OSDriver_Register(rpl_entry.r3, 150, ptr, 3) KPAD
OSDriver_Register(rpl_entry.r3, 50, ptr, 2) WPAD
OSDriver_Register(rpl_entry.r3, 200, ptr, 0) ProcUI
...
*/

BOOL
OSDriver_Register(uint32_t moduleHandle,
                  uint32_t inUnk1, // Something like a time duration or priority
                  virt_ptr<OSDriverInterface> interface,
                  virt_ptr<void> userData,
                  virt_ptr<uint32_t> outUnk1,
                  virt_ptr<uint32_t> outUnk2,
                  virt_ptr<uint32_t> outUnk3)
{
   // TODO: OSDriver_Register
   return FALSE;
}

BOOL
OSDriver_Deregister(uint32_t moduleHandle,
                    virt_ptr<void> userData)
{
   // TODO: OSDriver_Deregister
   return FALSE;
}

void
Library::registerDriverFunctions()
{
   RegisterFunctionExport(OSDriver_Register);
   RegisterFunctionExport(OSDriver_Deregister);
}

} // namespace cafe::coreinit

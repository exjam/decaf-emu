#include "coreinit.h"
#include "coreinit_driver.h"

namespace cafe::coreinit
{

struct StaticFsDriverData
{
   be2_struct<OSDriverInterface> interface;
   be2_array<char, 4> name = "FS";
   be2_val<BOOL> isDone = FALSE;
};

virt_ptr<StaticFsDriverData>
getFsDriverData()
{
   return Library::getStaticData()->fsDriverData;
}

namespace internal
{

static virt_ptr<const char>
fsDriverName()
{
   return virt_addrof(getFsDriverData()->name);
}

void
fsDriverOnInit(virt_ptr<void> context)
{
}

void
fsDriverOnAcquiredForeground(virt_ptr<void> context)
{
}

void
fsDriverOnReleasedForeground(virt_ptr<void> context)
{
}

void
fsDriverOnDone(virt_ptr<void> context)
{
   getFsDriverData()->isDone = TRUE;
}

bool
fsDriverDone()
{
   return !!getFsDriverData()->isDone;
}

} // namespace internal

void
Library::initialiseFsDriverStaticData()
{
   auto fsDriverData = allocStaticData<StaticFsDriverData>();
   getStaticData()->fsDriverData = fsDriverData;

   fsDriverData->interface.getName = GetInternalFunctionAddress(internal::fsDriverName);
   fsDriverData->interface.onInit = GetInternalFunctionAddress(internal::fsDriverOnInit);
   fsDriverData->interface.onAcquiredForeground = GetInternalFunctionAddress(internal::fsDriverOnAcquiredForeground);
   fsDriverData->interface.onReleasedForeground = GetInternalFunctionAddress(internal::fsDriverOnReleasedForeground);
   fsDriverData->interface.onDone = GetInternalFunctionAddress(internal::fsDriverOnDone);
}

void
Library::registerFsDriverFunctions()
{
   RegisterFunctionInternal(internal::fsDriverName);
   RegisterFunctionInternal(internal::fsDriverOnInit);
   RegisterFunctionInternal(internal::fsDriverOnAcquiredForeground);
   RegisterFunctionInternal(internal::fsDriverOnReleasedForeground);
   RegisterFunctionInternal(internal::fsDriverOnDone);
}

} // namespace cafe::coreinit

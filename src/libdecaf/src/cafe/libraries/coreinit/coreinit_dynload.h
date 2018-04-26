#pragma once
#include "coreinit_enum.h"
#include <libcpu/be2_struct.h>

namespace cafe::coreinit
{

struct OSDynLoad_NotifyData;

using OSDynLoad_ModuleHandle = uint32_t;

using OSDynLoad_AllocFn = virt_func_ptr<
   OSDynLoad_Error(int32_t size,
                  int32_t align,
                  virt_ptr<virt_ptr<void>> out)>;

using OSDynLoad_FreeFn = virt_func_ptr<
   void(virt_ptr<void> ptr)>;

using OSDynLoad_NotifyCallbackFn = virt_func_ptr<
   void(OSDynLoad_ModuleHandle handle,
        virt_ptr<void> userArg1,
        OSDynLoad_NotifyEvent event,
        virt_ptr<OSDynLoad_NotifyData>)>;

struct OSDynLoad_NotifyData
{
   be2_virt_ptr<char> name;

   be2_val<virt_addr> codeStart;
   be2_val<uint32_t> codeOffset;
   be2_val<uint32_t> codeSize;

   be2_val<virt_addr> dataStart;
   be2_val<uint32_t> dataOffset;
   be2_val<uint32_t> dataSize;

   be2_val<virt_addr> rodataStart;
   be2_val<uint32_t> rodataOffset;
   be2_val<uint32_t> rodataSize;
};
CHECK_OFFSET(OSDynLoad_NotifyData, 0x00, name);
CHECK_OFFSET(OSDynLoad_NotifyData, 0x04, codeStart);
CHECK_OFFSET(OSDynLoad_NotifyData, 0x08, codeOffset);
CHECK_OFFSET(OSDynLoad_NotifyData, 0x0C, codeSize);
CHECK_OFFSET(OSDynLoad_NotifyData, 0x10, dataStart);
CHECK_OFFSET(OSDynLoad_NotifyData, 0x14, dataOffset);
CHECK_OFFSET(OSDynLoad_NotifyData, 0x18, dataSize);
CHECK_OFFSET(OSDynLoad_NotifyData, 0x1C, rodataStart);
CHECK_OFFSET(OSDynLoad_NotifyData, 0x20, rodataOffset);
CHECK_OFFSET(OSDynLoad_NotifyData, 0x24, rodataSize);
CHECK_SIZE(OSDynLoad_NotifyData, 0x28);

struct OSDynLoad_NotifyCallback
{
   UNKNOWN(0x4);
   be2_val<OSDynLoad_NotifyCallbackFn> notifyFn;
   be2_virt_ptr<void> userArg1;
   be2_virt_ptr<OSDynLoad_NotifyCallback> next;
};
CHECK_SIZE(OSDynLoad_NotifyCallback, 0x10);

OSDynLoad_Error
OSDynLoad_AddNotifyCallback(OSDynLoad_NotifyCallbackFn notifyFn,
                            virt_ptr<void> userArg1);

void
OSDynLoad_DelNotifyCallback(OSDynLoad_NotifyCallbackFn notifyFn,
                            virt_ptr<void> userArg1);

OSDynLoad_Error
OSDynLoad_GetAllocator(virt_ptr<OSDynLoad_AllocFn> outAllocFn,
                       virt_ptr<OSDynLoad_FreeFn> outFreeFn);

OSDynLoad_Error
OSDynLoad_SetAllocator(OSDynLoad_AllocFn allocFn,
                       OSDynLoad_FreeFn freeFn);

OSDynLoad_Error
OSDynLoad_GetTLSAllocator(virt_ptr<OSDynLoad_AllocFn> outAllocFn,
                          virt_ptr<OSDynLoad_FreeFn> outFreeFn);

OSDynLoad_Error
OSDynLoad_SetTLSAllocator(OSDynLoad_AllocFn allocFn,
                          OSDynLoad_FreeFn freeFn);

OSDynLoad_Error
OSDynLoad_Acquire(virt_ptr<char> modulePath,
                  virt_ptr<OSDynLoad_ModuleHandle> outModuleHandle);

OSDynLoad_Error
OSDynLoad_AcquireContainingModule(virt_ptr<void> ptr,
                                  OSDynLoad_SectionType sectionType,
                                  virt_ptr<OSDynLoad_ModuleHandle> outHandle);

OSDynLoad_Error
OSDynLoad_Release(OSDynLoad_ModuleHandle moduleHandle);

} // namespace cafe::coreinit

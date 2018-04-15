#pragma once
#include "coreinit_enum.h"
#include <libcpu/be2_struct.h>

namespace cafe::coreinit
{

using OSDynLoad_AllocFn = virt_func_ptr<
   OSDynLoadError (int32_t size,
                   int32_t align,
                   virt_ptr<virt_ptr<void>> out)>;

using OSDynLoad_FreeFn = virt_func_ptr<
   void (virt_ptr<void> ptr)>;

OSDynLoadError
OSDynLoad_GetAllocator(virt_ptr<OSDynLoad_AllocFn> outAllocFn,
                       virt_ptr<OSDynLoad_FreeFn> outFreeFn);

OSDynLoadError
OSDynLoad_SetAllocator(OSDynLoad_AllocFn allocFn,
                       OSDynLoad_FreeFn freeFn);

OSDynLoadError
OSDynLoad_GetTLSAllocator(virt_ptr<OSDynLoad_AllocFn> outAllocFn,
                          virt_ptr<OSDynLoad_FreeFn> outFreeFn);

OSDynLoadError
OSDynLoad_SetTLSAllocator(OSDynLoad_AllocFn allocFn,
                          OSDynLoad_FreeFn freeFn);

} // namespace cafe::coreinit

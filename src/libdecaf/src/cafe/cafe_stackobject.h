#pragma once
#include <algorithm>
#include <common/align.h>
#include <common/decaf_assert.h>
#include <libcpu/cpu.h>
#include <libcpu/be2_struct.h>
#include <memory>

namespace cafe
{

template<typename Type, size_t NumElements = 1>
class StackObject : public virt_ptr<Type>
{
   static constexpr auto
   AlignedSize = align_up(static_cast<uint32_t>(sizeof(Type) * NumElements),
                          std::max<std::size_t>(alignof(Type), 4u));

public:
   StackObject()
   {
      auto core = cpu::this_core::state();
      auto oldStackTop = virt_addr { core->gpr[1] };
      auto newStackTop = oldStackTop - AlignedSize;
      auto ptrAddr = newStackTop + 8;

      memmove(virt_cast<void *>(newStackTop).getRawPointer(),
              virt_cast<void *>(oldStackTop).getRawPointer(),
              8);

      core->gpr[1] = newStackTop.getAddress();
      mAddress = virt_addr { newStackTop + 8 };
      std::uninitialized_default_construct_n(getRawPointer(), NumElements);
   }

   ~StackObject()
   {
      std::destroy_n(getRawPointer(), NumElements);

      auto core = cpu::this_core::state();
      auto oldStackTop = virt_addr { core->gpr[1] };
      auto newStackTop = virt_addr { core->gpr[1] + AlignedSize };
      auto ptrAddr = oldStackTop + 8;
      decaf_check(mAddress == ptrAddr);

      core->gpr[1] = newStackTop.getAddress();
      memmove(virt_cast<void *>(newStackTop).getRawPointer(),
              virt_cast<void *>(oldStackTop).getRawPointer(),
              8);
   }
};

} // namespace cafe

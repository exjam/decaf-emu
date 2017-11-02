#pragma once
#include <common/align.h>
#include <common/decaf_assert.h>
#include <libcpu/cpu.h>
#include <libcpu/be2_struct.h>

namespace cafe
{

template<typename Type>
class StackObject : public virt_ptr<Type>
{
   static constexpr auto AlignedSize = align_up(static_cast<uint32_t>(sizeof(Type)), std::max(alignof(Type), 4));

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
      new (getRawPointer()) Type { };
   }

   ~StackObject()
   {
      auto core = cpu::this_core::state();
      auto oldStackTop = virt_addr { core->gpr[1] };
      auto newStackTop = virt_addr { core->gpr[1] + AlignedSize };
      auto ptrAddr = oldStackTop + 8;
      decaf_check(mAddress == ptrAddr);

      getRawPointer()->~Type();

      core->gpr[1] = newStackTop.getAddress();
      memmove(virt_cast<void *>(newStackTop).getRawPointer(),
              virt_cast<void *>(oldStackTop).getRawPointer(),
              8);
   }
};

} // namespace cafe

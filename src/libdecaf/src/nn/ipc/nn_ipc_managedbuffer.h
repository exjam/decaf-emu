#pragma once
#include <libcpu/be2_struct.h>

namespace nn::ipc
{

struct ManagedBuffer
{
   ManagedBuffer(virt_ptr<void> ptr, uint32_t size, bool input, bool output) :
      ptr(ptr),
      size(size),
      input(input),
      output(output)
   {
   }

   virt_ptr<void> ptr;
   uint32_t size;
   bool input;
   bool output;
};

template<typename Type>
struct InBuffer : ManagedBuffer
{
   InBuffer(virt_ptr<Type> ptr) :
      ManagedBuffer(ptr, static_cast<uint32_t>(sizeof(Type)), true, false)
   {
   }

   InBuffer(virt_ptr<Type> ptr, uint32_t count) :
      ManagedBuffer(ptr, static_cast<uint32_t>(sizeof(Type) * count), true, false)
   {
   }
};

template<>
struct InBuffer<void> : ManagedBuffer
{
   InBuffer(virt_ptr<void> ptr, uint32_t count) :
      ManagedBuffer(ptr, count, true, false)
   {
   }
};

template<typename Type>
struct InOutBuffer : ManagedBuffer
{
   InOutBuffer(virt_ptr<Type> ptr) :
      ManagedBuffer(ptr, static_cast<uint32_t>(sizeof(Type)), true, true)
   {
   }

   InOutBuffer(virt_ptr<Type> ptr, uint32_t count) :
      ManagedBuffer(ptr, static_cast<uint32_t>(sizeof(Type) * count), true, true)
   {
   }
};

template<>
struct InOutBuffer<void> : ManagedBuffer
{
   InOutBuffer(virt_ptr<void> ptr, uint32_t count) :
      ManagedBuffer(ptr, count, true, true)
   {
   }
};

template<typename Type>
struct OutBuffer : ManagedBuffer
{
   OutBuffer(virt_ptr<Type> ptr) :
      ManagedBuffer(ptr, static_cast<uint32_t>(sizeof(Type)), false, true)
   {
   }

   OutBuffer(virt_ptr<Type> ptr, uint32_t count) :
      ManagedBuffer(ptr, static_cast<uint32_t>(sizeof(Type) * count), false, true)
   {
   }
};

template<>
struct OutBuffer<void> : ManagedBuffer
{
   OutBuffer(virt_ptr<void> ptr, uint32_t count) :
      ManagedBuffer(ptr, count, false, true)
   {
   }
};

} // namespace nn::ipc

#pragma once
#include <libcpu/be2_struct.h>

namespace cafe::kernel
{

void
startKernel();

namespace internal
{

virt_ptr<void>
allocStatic(std::size_t size, std::size_t alignment);

template<typename Type>
inline virt_ptr<Type>
allocStatic()
{
   return virt_cast<Type *>(allocStatic(sizeof(Type), alignof(Type)));
}

} // namespace internal

} // namespace cafe::kernel

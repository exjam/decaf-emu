#pragma once
#include <libcpu/be2_struct.h>

namespace cafe::kernel
{

phys_addr
virtualToPhysical(virt_addr pa);

virt_addr
physicalToVirtual(phys_addr pa);

template<typename Type>
virt_ptr<Type>
physicalToVirtual(phys_ptr<Type> ptr)
{
   return virt_cast<Type *>(physicalToVirtual(phys_cast<phys_addr>(ptr)));
}

template<typename Type>
phys_ptr<Type>
virtualToPhysical(virt_ptr<Type> ptr)
{
   return phys_cast<Type *>(virtualToPhysical(virt_cast<virt_addr>(ptr)));
}

template<typename Type>
virt_ptr<Type>
physicalToVirtual(be2_phys_ptr<Type> ptr)
{
   return virt_cast<Type *>(physicalToVirtual(phys_cast<phys_addr>(ptr)));
}

template<typename Type>
phys_ptr<Type>
virtualToPhysical(be2_virt_ptr<Type> ptr)
{
   return phys_cast<Type *>(virtualToPhysical(virt_cast<virt_addr>(ptr)));
}

} // namespace cafe::kernel

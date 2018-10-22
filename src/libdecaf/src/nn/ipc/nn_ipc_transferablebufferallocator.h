#pragma once
#include "cafe/libraries/coreinit/coreinit_ios.h"
#include "cafe/libraries/coreinit/coreinit_mutex.h"

#include <libcpu/be2_struct.h>

namespace nn::ipc
{

class TransferableBufferAllocator
{
   static constexpr auto BufferSize = 256u;

   struct FreeBuffer
   {
      be2_virt_ptr<FreeBuffer> next;
   };

public:
   TransferableBufferAllocator();

   void
   Initialize(virt_ptr<void> buffer,
              uint32_t size);

   virt_ptr<void>
   Allocate(uint32_t size);

   void
   Deallocate(virt_ptr<void> ptr,
              uint32_t size);

private:
   be2_struct<cafe::coreinit::OSMutex> mMutex;
   be2_virt_ptr<FreeBuffer> mFreeBuffers;
};

} // namespace nn::ipc

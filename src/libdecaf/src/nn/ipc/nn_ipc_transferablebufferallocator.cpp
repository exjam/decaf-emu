#pragma optimize("", off)
#include "nn_ipc_transferablebufferallocator.h"
#include "cafe/libraries/coreinit/coreinit_ios.h"
#include "cafe/libraries/coreinit/coreinit_mutex.h"

using namespace cafe::coreinit;

namespace nn::ipc
{

TransferableBufferAllocator::TransferableBufferAllocator()
{
   OSInitMutex(virt_addrof(mMutex));
}

void
TransferableBufferAllocator::Initialize(virt_ptr<void> buffer,
                                        uint32_t size)
{
   auto numBuffers = size / BufferSize;

   // Set up a linked list of free buffers
   auto bufferAddr = virt_cast<virt_addr>(buffer);
   mFreeBuffers = virt_cast<FreeBuffer *>(bufferAddr);

   for (auto i = 0u; i < numBuffers; ++i) {
      auto curBuffer = virt_cast<FreeBuffer *>(bufferAddr);
      auto nextBuffer = virt_cast<FreeBuffer *>(bufferAddr + BufferSize);

      if (i < numBuffers - 1) {
         curBuffer->next = nextBuffer;
      } else {
         curBuffer->next = nullptr;
      }

      bufferAddr += BufferSize;
   }
}

virt_ptr<void>
TransferableBufferAllocator::Allocate(uint32_t size)
{
   auto result = virt_ptr<void> { nullptr };
   decaf_check(size <= BufferSize);

   OSLockMutex(virt_addrof(mMutex));
   result = mFreeBuffers;
   mFreeBuffers = mFreeBuffers->next;
   OSUnlockMutex(virt_addrof(mMutex));
   return result;
}

void
TransferableBufferAllocator::Deallocate(virt_ptr<void> ptr,
                                        uint32_t size)
{
   auto buffer = virt_cast<FreeBuffer *>(ptr);
   decaf_check(size <= BufferSize);

   OSLockMutex(virt_addrof(mMutex));
   buffer->next = mFreeBuffers;
   mFreeBuffers = buffer;
   OSUnlockMutex(virt_addrof(mMutex));
}

} // namespace nn::ipc

#pragma once
#include "ios/kernel/ios_kernel_thread.h"

namespace ios::nn
{

class Thread
{
public:
   using id = kernel::ThreadId;
   using native_handle_type = kernel::ThreadId;

   Thread() noexcept = default;
   Thread(const Thread &) = delete;
   Thread(Thread &&other) noexcept;
   ~Thread();

   Error start(kernel::ThreadEntryFn entry,
              phys_ptr<void> context,
              phys_ptr<uint8_t> stackTop,
              uint32_t stackSize,
              kernel::ThreadPriority priority);
   void join();
   void detach();
   void swap(Thread &other) noexcept;

   id get_id() const noexcept;
   native_handle_type native_handle() const noexcept;
   bool joinable() const noexcept;

   static unsigned int hardware_concurrency() noexcept
   {
      return 1;
   }

private:
   kernel::ThreadId mThreadId = -1;
   bool mJoined = true;
};

} // namespace ios::nn

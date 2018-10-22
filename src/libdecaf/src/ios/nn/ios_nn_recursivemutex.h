#pragma once
#include "ios_nn_criticalsection.h"

#include "ios/kernel/ios_kernel_thread.h"

namespace ios::nn
{

class RecursiveMutex
{
public:
   void lock();
   bool try_lock();
   void unlock();

   bool locked();

private:
   int mRecursionCount = 0;
   kernel::ThreadId mOwnerThread = -1;
   CriticalSection mCriticalSection;
};

} // namespace ios::nn

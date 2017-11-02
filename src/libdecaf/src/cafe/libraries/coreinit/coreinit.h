#pragma once
#include "cafe/libraries/cafe_hle_library.h"
#include <libcpu/be2_struct.h>

namespace cafe::coreinit
{

struct StaticThreadData;

struct StaticData
{
   virt_ptr<StaticThreadData> thread;
};

class Library : public cafe::hle::Library
{
public:
   Library() :
      hle::Library(hle::LibraryId::coreinit)
   {
   }

   static void RegisterExports();
   void libraryEntryPoint() override;

private:
   void CafeInit();
};

} // namespace cafe::coreinit

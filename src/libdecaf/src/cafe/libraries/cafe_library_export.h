#pragma once
#include <libcpu/pointer.h>
#include <string>

namespace cpu
{
struct Core;
}

namespace cafe
{

struct LibraryExport
{
   enum Type
   {
      Undefined,
      Function,
      UnimplementedFunction,
      Data
   };

   //! Symbol type
   Type type = Undefined;

   //! Symbol name
   std::string name;
};

struct LibraryData : LibraryExport
{
};

struct LibraryFunction : LibraryExport
{
   virtual void call(cpu::Core *thread) = 0;
};

struct LibraryUnimplementedFunction : LibraryExport
{
   //! Module which this symbol comes from
   std::string module;
};

} // namespace cafe

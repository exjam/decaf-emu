#pragma once
#include <string>

namespace cpu
{
struct Core;
}

namespace cafe::hle
{

struct LibrarySymbol
{
   enum Type
   {
      Undefined,
      Function,
      UnimplementedFunction,
      Data
   };

   LibrarySymbol(Type type) :
      type(type)
   {
   }

   //! Symbol type
   Type type = Undefined;

   //! Symbol name
   std::string name;

   //! Whether the symbol is exported or not
   bool exported;
};

struct LibraryDataSymbol : LibrarySymbol
{
   LibraryDataSymbol() :
      LibrarySymbol(LibrarySymbol::Data)
   {
   }

   virt_ptr<void> ptr;
};

struct LibraryUnimplementedFunction : LibrarySymbol
{
   LibraryUnimplementedFunction() :
      LibrarySymbol(LibrarySymbol::UnimplementedFunction)
   {
   }

   //! Module which this symbol comes from
   std::string module;
};

} // namespace cafe

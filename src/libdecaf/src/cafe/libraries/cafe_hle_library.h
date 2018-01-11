#pragma once
#include "cafe_hle_library_symbol.h"
#include "cafe_hle_library_function.h"

#include <common/decaf_assert.h>
#include <map>

/*
#define RegisterKernelFunction(fn) \
   RegisterKernelFunctionName(#fn, fn)

#define RegisterKernelData(data) \
   RegisterKernelDataName(#data, data)

#define RegisterKernelFunctionConstructor(name, cls) \
   _RegisterKernelFunctionConstructor<cls>(name)

#define RegisterKernelFunctionConstructorArgs(name, cls, ...) \
   _RegisterKernelFunctionConstructor<cls, __VA_ARGS__>(name)

#define RegisterKernelFunctionDestructor(name, cls) \
   _RegisterKernelFunctionDestructor<cls>(name);

#define RegisterInternalFunction(fn, ...) \
   RegisterInternalFunctionName(#fn, fn, __VA_ARGS__)

#define RegisterInternalData(data) \
   RegisterInternalDataName(#data, data)

#define FindSymbol(sym) \
   findSymbol<decltype(sym)>(#sym)
*/

#define RegisterFunctionExport(fn) \
   registerFunctionExport(#fn, fn)

#define RegisterFunctionExportName(name, fn) \
   registerFunctionExport(name, fn)

#define RegisterDataExport(name, ptr) \
   registerDataExport(name, ptr)

#define RegisterFunctionInternal(fn) \
   registerFunctionInternal("__internal__" # fn, fn)

#define GetInternalFunctionAddress(fn) \
   virt_func_cast<decltype(fn)>(getSymbolAddress("__internal__" # fn))

namespace cafe::hle
{

enum class LibraryId
{
   avm,
   camera,
   coreinit,
   dmae,
   erreula,
   gx2,
   mic,
   nn_ac,
   nn_acp,
   nn_act,
   nn_aoc,
   nn_boss,
   nn_cmpt,
   nn_fp,
   nn_ndm,
   nn_nfp,
   nn_olv,
   nn_save,
   nn_temp,
   nsyskbd,
   nsysnet,
   padscore,
   proc_ui,
   sci,
   snd_core,
   snd_user,
   swkbd,
   sysapp,
   tcl,
   vpad,
   zlib125
};

class Library
{
public:
   Library(LibraryId id) :
      mID(id)
   {
   }

   virtual ~Library() = default;
   virtual void libraryEntryPoint() = 0;

protected:
   virtual void registerExports() = 0;

   template<typename FunctionType>
   void
   registerFunctionInternal(const char *name,
                            FunctionType func)
   {
      registerSymbol(name, internal::makeLibraryFunction(func));
   }

   template<typename FunctionType>
   void
   registerFunctionExport(const char *name,
                          FunctionType func)
   {
      auto symbol = internal::makeLibraryFunction(func);
      symbol->exported = true;
      registerSymbol(name, std::move(symbol));
   }

   template<typename DataType>
   void
   registerDataExport(const char *name,
                      virt_ptr<DataType> data)
   {
      auto symbol = new LibraryDataSymbol { };
      symbol->exported = true;
      symbol->ptr = data;
      registerSymbol(name, { symbol });
   }

   void
   registerSymbol(const char *name,
                  std::unique_ptr<LibrarySymbol> symbol)
   {
      decaf_check(mSymbolMap.find(name) == mSymbolMap.end());
      symbol->name = name;
      mSymbolMap.emplace(name, std::move(symbol));
   }

   virt_addr
   getSymbolAddress(std::string_view name)
   {
      // TODO: Library::getSymbolAddress
      return virt_addr { 0 };
   }

private:
   LibraryId mID;
   std::map<const char *, std::unique_ptr<LibrarySymbol>> mSymbolMap;
};

} // namespace cafe::hle

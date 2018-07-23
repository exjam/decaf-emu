#pragma once
#include "cafe_hle_library_data.h"
#include "cafe_hle_library_function.h"
#include "cafe_hle_library_typeinfo.h"

#include <common/decaf_assert.h>
#include <map>
#include <string>
#include <vector>

#define RegisterEntryPoint(fn) \
   registerFunctionExport("rpl_entry", fn)

#define RegisterFunctionExport(fn) \
   registerFunctionExport(#fn, fn)

#define RegisterFunctionExportName(name, fn) \
   registerFunctionExport(name, fn)

#define RegisterDataExport(data) \
   registerDataExport(#data, data)

#define RegisterDataExportName(name, data) \
   registerDataExport(name, data)

#define RegisterConstructorExport(name, object) \
   registerConstructorExport<object>(name)

#define RegisterConstructorExportArgs(name, object, ...) \
   registerConstructorExport<object, __VA_ARGS__>(name)

#define RegisterDestructorExport(name, object) \
   registerDestructorExport<object>(name)

#define RegisterFunctionInternal(fn, ptr) \
   registerFunctionInternal("__internal__" # fn, fn, ptr)

#define RegisterDataInternal(data) \
   registerDataInternal("__internal__" # data, data)

namespace cafe::hle
{

enum class LibraryId
{
   avm,
   camera,
   coreinit,
   dc,
   dmae,
   drmapp,
   erreula,
   gx2,
   h264,
   lzma920,
   mic,
   nfc,
   nio_prof,
   nlibcurl,
   nlibnss2,
   nlibnss,
   nn_acp,
   nn_ac,
   nn_act,
   nn_aoc,
   nn_boss,
   nn_ccr,
   nn_cmpt,
   nn_dlp,
   nn_ec,
   nn_fp,
   nn_hai,
   nn_hpad,
   nn_idbe,
   nn_ndm,
   nn_nets2,
   nn_nfp,
   nn_nim,
   nn_olv,
   nn_pdm,
   nn_save,
   nn_sl,
   nn_spm,
   nn_temp,
   nn_uds,
   nn_vctl,
   nsysccr,
   nsyshid,
   nsyskbd,
   nsysnet,
   nsysuhs,
   nsysuvd,
   ntag,
   padscore,
   proc_ui,
   sndcore2,
   snd_core,
   snduser2,
   snd_user,
   swkbd,
   sysapp,
   tcl,
   tve,
   uac,
   uac_rpl,
   usb_mic,
   uvc,
   uvd,
   vpadbase,
   vpad,
   zlib125,
   Max,
};

class Library
{
   // We have 3 default suymbols: NULL, .text, .data
   static constexpr auto BaseSymbolIndex = uint32_t { 3 };

public:
   Library(LibraryId id, std::string name) :
      mID(id), mName(std::move(name))
   {
   }

   virtual ~Library() = default;

   LibraryId
   id() const
   {
      return mID;
   }

   const std::string &
   name() const
   {
      return mName;
   }

   virt_addr
   findSymbolAddress(std::string_view name) const
   {
      auto itr = mSymbolMap.find(name);
      if (itr == mSymbolMap.end()) {
         return virt_addr { 0 };
      }

      return itr->second->address;
   }

   LibrarySymbol *
   findSymbol(std::string_view name) const
   {
      auto itr = mSymbolMap.find(name);
      if (itr == mSymbolMap.end()) {
         return nullptr;
      }

      return itr->second.get();
   }

   const std::vector<uint8_t> &
   getGeneratedRpl() const
   {
      return mGeneratedRpl;
   }

   void
   generate()
   {
      registerSymbols();
      generateRpl();
   }

protected:
   virtual void
   registerSymbols() = 0;

   void
   generateRpl();

   // TODO: Check types match
   template<typename FunctionType>
   void
   registerFunctionInternal(const char *name,
                            FunctionType func,
                            virt_func_ptr<typename std::remove_pointer<FunctionType>::type> &hostPtr)
   {
      auto symbol = internal::makeLibraryFunction(func);
      symbol->exported = false;
      symbol->hostPtr = reinterpret_cast<virt_ptr<void> *>(&hostPtr);
      registerSymbol(name, std::move(symbol));
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

   template<typename ObjectType>
   void
   registerConstructorExport(const char *name)
   {
      auto symbol = internal::makeLibraryConstructorFunction<ObjectType>();
      symbol->exported = true;
      registerSymbol(name, std::move(symbol));
   }

   template<typename ObjectType>
   void
   registerDestructorExport(const char *name)
   {
      auto symbol = internal::makeLibraryDestructorFunction<ObjectType>();
      symbol->exported = true;
      registerSymbol(name, std::move(symbol));
   }

   template<typename DataType>
   void
   registerDataInternal(const char *name,
                        virt_ptr<DataType> &data)
   {
      auto symbol = std::make_unique<LibraryData>();
      symbol->exported = false;
      symbol->hostPointer = reinterpret_cast<virt_ptr<void> *>(&data);
      symbol->size = sizeof(DataType);
      registerSymbol(name, std::move(symbol));
   }

   template<typename DataType>
   void
   registerDataExport(const char *name,
                      virt_ptr<DataType> &data)
   {
      auto symbol = std::make_unique<LibraryData>();
      symbol->exported = true;
      symbol->hostPointer = reinterpret_cast<virt_ptr<void> *>(&data);
      symbol->size = sizeof(DataType);
      registerSymbol(name, std::move(symbol));
   }

   void
   registerSymbol(const char *name,
                  std::unique_ptr<LibrarySymbol> symbol)
   {
      decaf_check(mSymbolMap.find(name) == mSymbolMap.end());
      symbol->index = BaseSymbolIndex + static_cast<uint32_t>(mSymbolMap.size());
      symbol->name = name;
      mSymbolMap.emplace(name, std::move(symbol));
   }

   template<typename ObjectType>
   void
   registerTypeInfo(const char *typeName,
                    std::vector<const char *> &&virtualTable,
                    std::vector<const char *> &&baseTypes = {})
   {
      auto &typeInfo = mTypeInfo.emplace_back();
      typeInfo.name = typeName;
      typeInfo.virtualTable = std::move(virtualTable);
      typeInfo.baseTypes = std::move(baseTypes);
      typeInfo.hostVirtualTablePtr = &ObjectType::VirtualTable;
      typeInfo.hostTypeDescriptorPtr = &ObjectType::TypeDescriptor;
   }

private:
   LibraryId mID;
   std::string mName;
   std::map<std::string, std::unique_ptr<LibrarySymbol>, std::less<>> mSymbolMap;
   std::vector<LibraryTypeInfo> mTypeInfo;
   std::vector<uint8_t> mGeneratedRpl;
};

} // namespace cafe::hle

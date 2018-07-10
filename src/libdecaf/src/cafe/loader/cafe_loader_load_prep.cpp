#pragma optimize("", off)
#include "cafe_loader_bounce.h"
#include "cafe_loader_entry.h"
#include "cafe_loader_error.h"
#include "cafe_loader_globals.h"
#include "cafe_loader_heap.h"
#include "cafe_loader_iop.h"
#include "cafe_loader_load_basics.h"
#include "cafe_loader_load_prep.h"
#include "cafe_loader_load_shared.h"
#include "cafe_loader_log.h"
#include "cafe_loader_minfileinfo.h"
#include "cafe_loader_utils.h"
#include "cafe/cafe_stackobject.h"
#include "cafe/kernel/cafe_kernel_process.h"

namespace cafe::loader::internal
{

constexpr auto MaxModuleNameLen = 0x3Bu;

void
sUpdateFileInfoForUser(virt_ptr<LOADED_RPL> rpl,
                       virt_ptr<void> unk,
                       virt_ptr<LOADER_MinFileInfo> info)
{
   if (rpl->loadStateFlags & LoaderStateFlags_Unk0x20000000) {
      if (unk) {
         // *(unk + 0x16) |= 4u
      } else if (info) {
         info->fileInfoFlags |= LoaderStateFlagBit3;
      }
   }
}

static int32_t
LiGetMinFileInfo(virt_ptr<LOADED_RPL> rpl,
                 virt_ptr<LOADER_MinFileInfo> info)
{
   auto fileInfo = rpl->fileInfoBuffer;
   *info->outSizeOfFileInfo = rpl->fileInfoSize;

   if (fileInfo->runtimeFileInfoSize) {
      *info->outSizeOfFileInfo = fileInfo->runtimeFileInfoSize;
   }

   info->dataSize = fileInfo->dataSize;
   info->dataAlign = fileInfo->dataAlign;
   info->loadSize = fileInfo->loadSize;
   info->loadAlign = fileInfo->loadAlign;
   info->fileInfoFlags = fileInfo->flags;

   if (rpl->fileType == ios::mcp::MCPFileType::ProcessCode) {
      info->fileLocation = getProcTitleLoc();
   }

   if (info->inoutNextTlsModuleNumber && (fileInfo->flags & 8)) {
      if (fileInfo->tlsModuleIndex != -1) {
         Loader_ReportError("*** unexpected module index.\n");
         LiSetFatalError(0x18729Bu, rpl->fileType, 1, "LiGetMinFileInfo", 261);
         return -470064;
      }

      fileInfo->tlsModuleIndex = *info->inoutNextTlsModuleNumber;
      *info->inoutNextTlsModuleNumber += 1;
   }

   if (fileInfo->filename) {
      auto path = virt_cast<char *>(rpl->fileInfoBuffer) + fileInfo->filename;
      *info->outPathStringSize = static_cast<uint32_t>(strnlen(path.getRawPointer(), rpl->fileInfoSize - fileInfo->filename) + 1);
   } else {
      *info->outPathStringSize = 0u;
   }

   sUpdateFileInfoForUser(rpl, nullptr, info);
   return 0;
}

void
LiPurgeOneUnlinkedModule(virt_ptr<LOADED_RPL> rpl)
{
}

int32_t
LiLoadForPrep(virt_ptr<char> moduleName,
              uint32_t moduleNameLen,
              virt_ptr<void> chunkBuffer,
              virt_ptr<LOADED_RPL> *outLoadedRpl,
              BasicLoadArgs *loadArgs,
              uint32_t unk)
{
   auto globals = getGlobalStorage();
   auto rpl = virt_ptr<LOADED_RPL> { nullptr };
   auto error = LiLoadRPLBasics(moduleName,
                                moduleNameLen,
                                chunkBuffer,
                                globals->processCodeHeap,
                                globals->processCodeHeap,
                                true,
                                0,
                                &rpl,
                                loadArgs,
                                unk);
   if (error) {
      return error;
   }

   auto fileInfo = rpl->fileInfoBuffer;
   if (globals->loadedRpx && (fileInfo->flags & rpl::RPL_IS_RPX)) {
      Loader_ReportError("***Attempt to load RPX when main program already loaded.\n");
      LiSetFatalError(0x18729Bu, rpl->fileType, 1, "LiLoadForPrep", 1175);
      error = -470093;
      goto fail;
   }

   if (!globals->loadedRpx && !(fileInfo->flags & rpl::RPL_IS_RPX)) {
      Loader_ReportError("***Attempt to load non-RPX as main program.\n");
      LiSetFatalError(0x18729Bu, rpl->fileType, 1, "LiLoadForPrep", 1183);
      error = -470094;
      goto fail;
   }

   rpl->nextLoadedRpl = nullptr;
   if (globals->lastLoadedRpl) {
      globals->lastLoadedRpl->nextLoadedRpl = rpl;
      globals->lastLoadedRpl = rpl;
   } else {
      globals->firstLoadedRpl = rpl;
      globals->lastLoadedRpl = rpl;
   }

   *outLoadedRpl = rpl;
   return 0;

fail:
   if (rpl) {
      LiPurgeOneUnlinkedModule(rpl);
   }

   return error;
}

int32_t
LOADER_Prep(kernel::UniqueProcessId upid,
            virt_ptr<LOADER_MinFileInfo> minFileInfo)
{

   auto globals = getGlobalStorage();
   auto error = 0;

   LiCheckAndHandleInterrupts();

   if (globals->currentUpid != upid) {
      Loader_ReportError("*** Loader address space not set for process {} but called for {}.",
                         static_cast<int>(globals->currentUpid.value()),
                         static_cast<int>(upid));
      LiSetFatalError(0x18729Bu, 0, 1, "LOADER_Prep", 1262);
      LiCloseBufferIfError();
      return -470008;
   }

   if (minFileInfo) {
      if (auto error = LiValidateMinFileInfo(minFileInfo, "LOADER_Prep")) {
         LiCloseBufferIfError();
         return error;
      }
   }

   *minFileInfo->outKernelHandle = 0u;

   LiResolveModuleName(virt_addrof(minFileInfo->moduleNameBuffer),
                       virt_addrof(minFileInfo->moduleNameBufferLen));
   auto moduleName =
      std::string_view {
         minFileInfo->moduleNameBuffer.getRawPointer(),
         minFileInfo->moduleNameBufferLen
      };

   if (minFileInfo->moduleNameBufferLen == 8 &&
       strncmp(minFileInfo->moduleNameBuffer.getRawPointer(), "coreinit", 8) == 0) {
      Loader_ReportError("*** Loader Failure (system module re-load).");
      LiSetFatalError(0x18729Bu, 0, 1, "LOADER_Prep", 1305);
      LiCloseBufferIfError();
      return -470029;
   }

   for (auto itr = globals->firstLoadedRpl; itr; itr = itr->nextLoadedRpl) {
      if (strncmp(itr->moduleNameBuffer.getRawPointer(),
                  minFileInfo->moduleNameBuffer.getRawPointer(),
                  minFileInfo->moduleNameBufferLen) == 0) {
         Loader_ReportError("*** module \"{}\" already loaded.\n",
            std::string_view { minFileInfo->moduleNameBuffer.getRawPointer(),
                               minFileInfo->moduleNameBufferLen });
         LiSetFatalError(0x18729Bu, itr->fileType, 1, "LOADER_Prep", 1292);
         LiCloseBufferIfError();
         return -470028;
      }
   }

   // Check if module already loaded as a shared library
   if (!getProcFlags().disableSharedLibraries()) {
      auto sharedModule = findLoadedSharedModule(moduleName);
      if (sharedModule) {
         auto allocPtr = virt_ptr<void> { nullptr };
         auto allocSize = uint32_t { 0 };
         auto largestFree = uint32_t { 0 };
         error = LiCacheLineCorrectAllocEx(getGlobalStorage()->processCodeHeap,
                                           sizeof(LOADED_RPL),
                                           4,
                                           &allocPtr,
                                           1,
                                           &allocSize,
                                           &largestFree,
                                           sharedModule->fileType);
         if (error) {
            Loader_ReportError(
               "***Allocate Error {}, Failed to allocate {} bytes for system shared RPL tracking for {} in current process;  (needed {}, available {}).",
               error, allocSize, moduleName, allocSize, largestFree);
            LiCloseBufferIfError();
            return error;
         }

         auto trackingModule = virt_cast<LOADED_RPL *>(allocPtr);
         *trackingModule = *sharedModule;
         trackingModule->selfBufferSize = allocSize;
         trackingModule->globals = globals;
         trackingModule->loadStateFlags &= ~LoaderStateFlagBit3;

         // Add to global loaded module linked list
         trackingModule->nextLoadedRpl = nullptr;
         if (globals->lastLoadedRpl) {
            globals->lastLoadedRpl->nextLoadedRpl = trackingModule;
         } else {
            globals->firstLoadedRpl = trackingModule;
         }
         globals->lastLoadedRpl = trackingModule;

         *minFileInfo->outKernelHandle = virt_cast<virt_addr>(trackingModule->moduleNameBuffer);
         *minFileInfo->outNumberOfSections = trackingModule->elfHeader.shnum;

         error = LiGetMinFileInfo(trackingModule, minFileInfo);
         if (error) {
            LiCloseBufferIfError();
         }

         return error;
      }
   }

   StackArray<char, 64> filename;
   auto filenameLen = std::min<uint32_t>(minFileInfo->moduleNameBufferLen, 59);
   std::memcpy(filename.getRawPointer(), minFileInfo->moduleNameBuffer.getRawPointer(), filenameLen);
   std::strncpy(filename.getRawPointer() + filenameLen, ".rpl", filename.size() - filenameLen);

   LiCheckAndHandleInterrupts();
   LiInitBuffer(false);

   auto chunkBuffer = virt_ptr<void> { nullptr };
   auto chunkBufferSize = uint32_t { 0 };
   error = LiBounceOneChunk(filename.getRawPointer(),
                            ios::mcp::MCPFileType::CafeOS,
                            globals->currentUpid,
                            &chunkBufferSize,
                            0, 1,
                            &chunkBuffer);
   LiCheckAndHandleInterrupts();
   if (error) {
      Loader_ReportError(
         "***LiBounceOneChunk failed loading \"{}\" of type {} at offset 0x{:08X} err={}.",
         filename, 1, 0, error);
      LiCloseBufferIfError();
      return error;
   }

   auto loadArgs = BasicLoadArgs { };
   auto rpl = virt_ptr<LOADED_RPL> { nullptr };
   loadArgs.upid = upid;
   loadArgs.loadedRpl = nullptr;
   loadArgs.readHeapTracking = globals->processCodeHeap;
   loadArgs.pathNameLen = filenameLen;
   loadArgs.pathName = filename;
   loadArgs.fileType = ios::mcp::MCPFileType::CafeOS;
   loadArgs.chunkBuffer = chunkBuffer;
   loadArgs.chunkBufferSize = chunkBufferSize;
   loadArgs.fileOffset = 0u;

   error = LiLoadForPrep(filename,
                         filenameLen,
                         chunkBuffer,
                         &rpl,
                         &loadArgs,
                         0);
   if (error) {
      Loader_ReportError("***LiLoadForPrep failure {}. loading \"{}\".", error, filename);
      LiCloseBufferIfError();
      return error;
   }

   *minFileInfo->outKernelHandle = virt_cast<virt_addr>(rpl->moduleNameBuffer);
   *minFileInfo->outNumberOfSections = rpl->elfHeader.shnum;
   error = LiGetMinFileInfo(rpl, minFileInfo);
   if (error) {
      LiCloseBufferIfError();
   }

   return error;
}

} // namespace cafe::loader::internal

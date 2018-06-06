#if 0
#include "cafe_loader.h"
#include "cafe_loader_bounce.h"
#include "cafe_loader_iop.h"
#include "cafe_loader_ipcldriver.h"
#include "cafe/cafe_stackobject.h"
#include "cafe/cafe_tinyheap.h"
#include "cafe/kernel/cafe_kernel_process.h"
#include "cafe_loader_rpl.h"

#include <array>
#include <common/align.h>
#include <common/cbool.h>
#include <libcpu/cpu.h>
#include <libcpu/be2_struct.h>
#include <zlib.h>

namespace cafe::loader
{

struct RPL_STARTINFO
{
   UNKNOWN(0x4);
   be2_val<virt_addr> unk_0x04;
   UNKNOWN(0x1C);
   be2_val<virt_addr> unk_0x24;
};
CHECK_SIZE(RPL_STARTINFO, 0x28);

struct LoaderShared
{
   be2_val<uint32_t> unk0x00;
   be2_val<uint32_t> unk0x04;
   be2_val<uint32_t> unk0x08;
   be2_val<uint32_t> unk0x0C;
   be2_val<uint32_t> unk0x10;
   be2_val<uint32_t> unk0x14;
};
CHECK_SIZE(LoaderShared, 0x18);

struct StaticLoaderData
{
   // .data 0xEFE00000 - 0xEFE0A400
   be2_array<char, 0x1000> moduleName; // 0xEFE00000
   be2_val<kernel::UniqueProcessId> upid; // 0xEFE01000
   be2_virt_ptr<TinyHeap> codeAreaTracking; // 0xEFE01004
   be2_val<uint32_t> codeAreaTrackingSize; // 0xEFE01008
   be2_val<uint32_t> codeAreaNumBlocks; // 0xEFE0100C
   be2_val<uint32_t> codeAreaSize; // 0xEFE01010
   be2_val<uint32_t> maxCodeSize; // 0xEFE01014
   be2_val<uint32_t> unk_0xEFE01018; // 0xEFE01018

   // .bss EFE19E80
   be2_array<BOOL, 3> gIPCLInitialized; // 0xEFE13DFC

   be2_val<uint32_t> gProcFlags; // 0xEFE1AEA8
   be2_val<uint32_t> gProcTitleLoc; // 0xEFE1AEAC
   be2_val<uint32_t> gFatalErr; // 0xEFE1AEB0
   be2_val<uint32_t> gFatalLine; // 0xEFE1AEB4
   be2_val<uint32_t> gFatalMsgType; // 0xEFE1AEB8
   be2_virt_ptr<char> gFatalFunction; // 0xEFE1AEBC
   be2_virt_ptr<TinyHeap> gpSharedCodeHeapTracking; // 0xEFE1AEC0
   be2_virt_ptr<TinyHeap> gpSharedReadHeapTracking; // 0xEFE1AEC4
   be2_virt_ptr<LoaderShared> gpLoaderShared; // 0xEFE1AEC8

   be2_val<uint32_t> dataAreaSize; // 0xEFE1CEE0
   be2_val<uint32_t> dataAreaAllocations; // 0xEFE1CEE4
   be2_virt_ptr<TinyHeap> sgpTrackComp; // 0xEFE1CEE8
};

// Contained in 0xEFE0A400 - 0xEFE0A4C0
struct IpcData
{
   UNKNOWN(4); // 0xEFE0A400
   be2_val<uint32_t> procFlags; // 0xEFE0A404
   UNKNOWN(4); // 0xEFE0A408
   be2_val<kernel::UniqueProcessId> upid; // 0xEFE0A40C
   be2_val<uint32_t> num_codearea_heap_blocks; // 0xEFE0A410
   be2_val<uint32_t> max_codesize; // 0xEFE0A414
   be2_val<uint32_t> unk_0xEFE0A418;
   be2_val<uint32_t> procTitleLoc; // 0xEFE0A41C
   be2_val<uint32_t> unk_0xEFE0A420;
   be2_val<uint32_t> unk_0xEFE0A424;
   UNKNOWN(4); // 0xEFE0A428
   be2_val<uint32_t> fatalMsgType; // 0xEFE0A42C
   be2_val<uint32_t> fatalErr; // 0xEFE0A430
   be2_val<uint32_t> loaderInitResult; // 0xEFE0A434
   be2_val<uint32_t> fatalLine; // 0xEFE0A438
   be2_array<char, 64> fatalFunction; // 0xEFE0A43C
   be2_struct<RPL_STARTINFO> rplStartInfo; // 0xEFE0A47C
   UNKNOWN(0xEFE0A4C0 - 0xEFE0A4A4);
};
CHECK_SIZE(IpcData, 0xEFE0A4C0 - 0xEFE0A400);

virt_ptr<IpcData>
gIpcData;

virt_ptr<StaticLoaderData>
sData;

constexpr auto MaxCodeSize = 224u * 1024 * 1024;

// LOADER_Entry
void
entry(void *params)
{

}

constexpr auto DataAreaBase = virt_addr { 0x10000000 };
constexpr auto SharedCodeTrackingSize = 0x830;
constexpr auto SharedReadTrackingSize = 0x1030;

namespace internal
{

void
LiResolveModuleName(virt_ptr<char> *moduleName,
                    uint32_t *moduleNameLen)
{
   auto path = std::string_view { moduleName->getRawPointer(), *moduleNameLen };
   auto prefix = path.find_last_of("\/");
   if (prefix != std::string_view::npos) {
      *moduleName += prefix;
      *moduleNameLen -= prefix;
   }

   auto suffix = path.find_last_of(".");
   if (suffix != std::string_view::npos) {
      *moduleNameLen -= suffix;
   }
}

int32_t
ELFFILE_ValidateAndPrepareMinELF(virt_ptr<void> chunkBuffer,
                                 uint32_t chunkReadSize,
                                 virt_ptr<rpl::Header> *outElfHeader,
                                 virt_ptr<rpl::SectionHeader> *outSectionHeaders,
                                 uint32_t *outShEntSize,
                                 uint32_t *outPhEntSize)
{
   if (chunkReadSize < 0x104) {
      return 0xBAD00018;
   }

   auto header = virt_cast<rpl::Header *>(chunkBuffer);
   if (outElfHeader) {
      *outElfHeader = header;
   }

   if (header->magic != rpl::Header::Magic) {
      return 0xBAD00019;
   }

   if (header->fileClass != rpl::ELFCLASS32) {
      return 0xBAD0001A;
   }

   if (header->elfVersion != rpl::EV_CURRENT) {
      return 0xBAD0001B;
   }

   decaf_check(header->encoding == rpl::ELFDATA2MSB);

   if (header->machine != rpl::EM_PPC) {
      return 0xBAD0001C;
   }

   if (header->version != 1) {
      return 0xBAD0001D;
   }

   if (header->ehsize != sizeof(rpl::Header) ||
       (chunkReadSize - 0xD0) < header->ehsize) {
      return 0xBAD0001E;
   }

   if (header->phoff) {
      if (header->phoff < sizeof(rpl::Header) ||
          header->phoff < chunkReadSize) {
         return 0xBAD0001F;
      }
   }

   if (header->shoff) {
      if (header->phoff < sizeof(rpl::Header) ||
          header->phoff < chunkReadSize) {
         return 0xBAD00020;
      }
   }

   if (header->shstrndx) {
      if (header->shstrndx > header->shnum) {
         return 0xBAD00021;
      }
   }

   auto phEntSize = header->phentsize;
   if (!phEntSize) {
      phEntSize = sizeof(rpl::ProgramHeader);
   }

   if (outPhEntSize) {
      *outPhEntSize = phEntSize;
   }

   if (header->phoff) {
      if (header->phoff + (header->phnum * phEntSize) >= chunkReadSize) {
         return 0xBAD00022;
      }
   }

   auto shEntSize = header->shentsize;
   if (!shEntSize) {
      shEntSize = sizeof(rpl::SectionHeader);
   }

   if (outShEntSize) {
      *outShEntSize = shEntSize;
   }

   if (header->shoff) {
      if (header->shoff + (header->shnum * shEntSize) >= chunkReadSize) {
         return 0xBAD00023;
      }
   }

   auto sectionHeaders = virt_cast<rpl::SectionHeader *>(virt_cast<virt_addr>(chunkBuffer) + header->shoff);
   if (outSectionHeaders) {
      *outSectionHeaders = sectionHeaders;
   }

   for (auto i = 1; i < header->shnum; ++i) {
      auto sectionHeader = virt_cast<rpl::SectionHeader *>(virt_cast<virt_addr>(sectionHeaders) + shEntSize * i);
      if (sectionHeader->size == 0) {
         continue;
      }

      if (sectionHeader->type == rpl::SHT_NOBITS) {
         continue;
      }

      if (sectionHeader->offset >= chunkReadSize) {
         return 0xBAD00024;
      }

      if (header->phoff && header->phnum) {
         // TODO: Verify something with sectionHeader->info
      }

      if (header->shoff) {
         // TODO: Verify something with sectionHeader->info
      }

      for (auto j = i + 1; j < header->shnum; ++j) {
         // TODO: Verify something with size+offset and sectionHeader->info
      }
   }

   // TODO: Verify program headers
   return 0;
}

struct RPLBasics
{
   be2_val<cafe::kernel::UniqueProcessId> upid0x00;
   be2_val<uint32_t> selfBufferSize;
   be2_virt_ptr<void> moduleNameBuffer;
   be2_val<uint32_t> moduleNameLen;
   be2_val<uint32_t> moduleNameBufferSize;
   be2_virt_ptr<void> pathBuffer;
   be2_val<uint32_t> pathBufferSize;
   be2_struct<rpl::Header> elfHeader;
   be2_virt_ptr<void> sectionHeaderBuffer;
   be2_val<uint32_t> sectionHeaderBufferSize;
   be2_virt_ptr<void> rplFileInfoBuffer;
   be2_val<uint32_t> rplFileInfoSize;
   be2_val<uint32_t> rplFileInfoBufferSize;
   be2_virt_ptr<void> rplCrcBuffer;
   be2_val<uint32_t> rplCrcBufferSize;
   UNKNOWN(0x4);
   be2_val<uint32_t> unk0x70;
   UNKNOWN(0x4);
   be2_val<uint32_t> upcomingBufferNumber;
   be2_virt_ptr<void> firstChunkBuffer;
   be2_val<uint32_t> fileReadSize;
   be2_val<uint32_t> fileOffset;
   be2_val<uint32_t> upcomingFileOffset;
   be2_val<ios::mcp::MCPFileType> fileType;
   be2_val<uint32_t> unk0x90;
   be2_virt_ptr<void> chunkBuffer;
   UNKNOWN(0xAC - 0x98);
   be2_val<cafe::kernel::UniqueProcessId> upid0xAC;
   be2_virt_ptr<rpl::Header> firstChunkElfHeader;
   be2_virt_ptr<void> textBuffer;
   be2_val<uint32_t> textBufferSize;
   be2_val<uint32_t> unk0xBC;
   be2_virt_ptr<void> sharedLibraryLoadBuffer;
   UNKNOWN(0xF4 - 0xC4);
   be2_virt_ptr<void> sectionAddressBuffer;
   be2_val<uint32_t> sectionAddressBufferSize;
   UNKNOWN(0x118 - 0xFC);
};
CHECK_OFFSET(RPLBasics, 0x00, upid0x00);
CHECK_OFFSET(RPLBasics, 0x04, selfBufferSize);
CHECK_OFFSET(RPLBasics, 0x08, moduleNameBuffer);
CHECK_OFFSET(RPLBasics, 0x0C, moduleNameLen);
CHECK_OFFSET(RPLBasics, 0x10, moduleNameBufferSize);
CHECK_OFFSET(RPLBasics, 0x14, pathBuffer);
CHECK_OFFSET(RPLBasics, 0x18, pathBufferSize);
CHECK_OFFSET(RPLBasics, 0x1C, elfHeader);
CHECK_OFFSET(RPLBasics, 0x50, sectionHeaderBuffer);
CHECK_OFFSET(RPLBasics, 0x54, sectionHeaderBufferSize);
CHECK_OFFSET(RPLBasics, 0x58, rplFileInfoBuffer);
CHECK_OFFSET(RPLBasics, 0x5C, rplFileInfoSize);
CHECK_OFFSET(RPLBasics, 0x60, rplFileInfoBufferSize);
CHECK_OFFSET(RPLBasics, 0x64, rplCrcBuffer);
CHECK_OFFSET(RPLBasics, 0x68, rplCrcBufferSize);
CHECK_OFFSET(RPLBasics, 0x70, unk0x70);
CHECK_OFFSET(RPLBasics, 0x78, upcomingBufferNumber);
CHECK_OFFSET(RPLBasics, 0x7C, firstChunkBuffer);
CHECK_OFFSET(RPLBasics, 0x80, fileReadSize);
CHECK_OFFSET(RPLBasics, 0x84, fileOffset);
CHECK_OFFSET(RPLBasics, 0x88, upcomingFileOffset);
CHECK_OFFSET(RPLBasics, 0x8C, fileType);
CHECK_OFFSET(RPLBasics, 0x90, unk0x90);
CHECK_OFFSET(RPLBasics, 0x94, chunkBuffer);
CHECK_OFFSET(RPLBasics, 0xAC, upid0xAC);
CHECK_OFFSET(RPLBasics, 0xB0, firstChunkElfHeader);
CHECK_OFFSET(RPLBasics, 0xB4, textBuffer);
CHECK_OFFSET(RPLBasics, 0xB8, textBufferSize);
CHECK_OFFSET(RPLBasics, 0xBC, unk0xBC);
CHECK_OFFSET(RPLBasics, 0xC0, sharedLibraryLoadBuffer);
CHECK_OFFSET(RPLBasics, 0xF4, sectionAddressBuffer);
CHECK_OFFSET(RPLBasics, 0xF8, sectionAddressBufferSize);
CHECK_SIZE(RPLBasics, 0x118);

struct RPLBasicsLoadArgs
{
   be2_val<cafe::kernel::UniqueProcessId> upid;
   be2_virt_ptr<RPLBasics> rplBasics;
   be2_virt_ptr<TinyHeap> readHeapTracking;
   be2_val<uint32_t> pathNameLen;
   be2_virt_ptr<char> pathName;
   UNKNOWN(0x4);
   be2_val<ios::mcp::MCPFileType> fileType;
   be2_virt_ptr<void> chunkBuffer;
   be2_val<uint32_t> chunkBufferSize;
   be2_val<uint32_t> fileOffset;
};
CHECK_OFFSET(RPLBasicsLoadArgs, 0x00, upid);
CHECK_OFFSET(RPLBasicsLoadArgs, 0x04, rplBasics);
CHECK_OFFSET(RPLBasicsLoadArgs, 0x08, readHeapTracking);
CHECK_OFFSET(RPLBasicsLoadArgs, 0x0C, pathNameLen);
CHECK_OFFSET(RPLBasicsLoadArgs, 0x10, pathName);
CHECK_OFFSET(RPLBasicsLoadArgs, 0x18, fileType);
CHECK_OFFSET(RPLBasicsLoadArgs, 0x1C, chunkBuffer);
CHECK_OFFSET(RPLBasicsLoadArgs, 0x20, chunkBufferSize);
CHECK_OFFSET(RPLBasicsLoadArgs, 0x24, fileOffset);
CHECK_SIZE(RPLBasicsLoadArgs, 0x28);


void
LiSetFatalError(uint32_t baseError,
                ios::mcp::MCPFileType fileType,
                uint32_t unk,
                std::string_view function,
                uint32_t line)
{
   sData->gFatalErr = baseError;
   sData->gFatalMsgType = fileType;
   // sData->gFatalFunction = function;
   sData->gFatalLine = line;
}

void
LiResetFatalError()
{
   sData->gFatalErr = 0;
   sData->gFatalFunction = nullptr;
   sData->gFatalLine = 0;
   sData->gFatalMsgType = 0;
}

void
Loader_ReportInfo(const char *fmt, ...)
{
}

void
Loader_ReportError(const char *fmt, ...)
{
}

void
Loader_LogEntry(uint32_t unkR3,
                uint32_t unkR4,
                uint32_t unkR5,
                const char *fmt, ...)
{
}

int32_t
LiCacheLineCorrectAllocEx(virt_ptr<TinyHeap> heap,
                          uint32_t textSize,
                          int32_t textAlign,
                          virt_ptr<void> *outPtr,
                          uint32_t /*unused*/,
                          uint32_t *outAllocSize,
                          uint32_t *outLargestFree,
                          ios::mcp::MCPFileType fileType)
{
   auto fromEnd = false;
   textSize = align_up(textSize, 128);

   if (textAlign < 0) {
      textAlign = -textAlign;
      fromEnd = true;
   }

   if (textAlign == 0 && textAlign < 64) {
      textAlign = 64;
   }

   if (fromEnd) {
      textAlign = -textAlign;
   }

   auto tinyHeapError = TinyHeap_Alloc(heap, textSize, textAlign, outPtr);
   if (tinyHeapError < TinyHeapError::OK) {
      LiSetFatalError(0x187298, fileType, 0, "LiCacheLineCorrectAllocEx", 0x88);
      *outLargestFree = TinyHeap_GetLargestFree(heap);
      return static_cast<int32_t>(tinyHeapError);
   }

   std::memset(outPtr->getRawPointer(), 0, textSize);
   *outAllocSize = textSize;
   return 0;
}

int32_t
LiCacheLineCorrectAllocEx(virt_ptr<TinyHeap> heap,
                          uint32_t size,
                          int32_t align,
                          virt_ptr<virt_ptr<void>> outPtr,
                          uint32_t unused,
                          virt_ptr<uint32_t> outAllocSize,
                          uint32_t *outLargestFree,
                          ios::mcp::MCPFileType fileType)
{
   virt_ptr<void> tmpPtr;
   uint32_t tmpAllocSize;
   auto error = LiCacheLineCorrectAllocEx(heap,
                                          size,
                                          align,
                                          &tmpPtr,
                                          unused,
                                          &tmpAllocSize,
                                          outLargestFree,
                                          fileType);
   *outPtr = tmpPtr;
   *outAllocSize = tmpAllocSize;
   return error;
}

int32_t
LiCacheLineCorrectFreeEx(virt_ptr<TinyHeap> heap,
                         virt_ptr<void> ptr,
                         uint32_t size)
{
   TinyHeap_Free(heap, ptr);
}

int32_t
LiRefillUpcomingBounceBuffer(virt_ptr<RPLBasics> rplBasics,
                             int32_t bufferNumber)
{
   StackObject<uint32_t> chunkReadSize;
   return LiBounceOneChunk(virt_cast<char *>(rplBasics->pathBuffer).getRawPointer(),
                           rplBasics->fileType,
                           rplBasics->upid0xAC,
                           chunkReadSize,
                           rplBasics->upcomingFileOffset,
                           bufferNumber,
                           virt_addrof(rplBasics->chunkBuffer));
}

int32_t
LiInitBufferTracking(RPLBasicsLoadArgs *loadArgs)
{
   virt_ptr<void> allocPtr;
   uint32_t allocSize;
   uint32_t largestFree;
   auto error = LiCacheLineCorrectAllocEx(loadArgs->readHeapTracking,
                                          align_up(loadArgs->pathNameLen + 1, 4),
                                          4,
                                          &allocPtr,
                                          1,
                                          &allocSize,
                                          &largestFree,
                                          loadArgs->fileType);
   if (error != 0) {
      return error;
   }

   auto rplBasics = loadArgs->rplBasics;
   rplBasics->pathBuffer = allocPtr;
   rplBasics->pathBufferSize = allocSize;
   std::strncpy(virt_cast<char *>(rplBasics->pathBuffer).getRawPointer(),
                loadArgs->pathName.getRawPointer(),
                loadArgs->pathNameLen + 1);

   rplBasics->upcomingBufferNumber = 1;
   rplBasics->fileOffset = loadArgs->fileOffset;
   rplBasics->unk0x90 = loadArgs->chunkBufferSize;
   rplBasics->upid0xAC = loadArgs->upid;

   rplBasics->firstChunkBuffer = loadArgs->chunkBuffer;
   rplBasics->fileReadSize = loadArgs->chunkBufferSize;
   rplBasics->upcomingFileOffset = loadArgs->chunkBufferSize;
   rplBasics->fileType = loadArgs->fileType;

   if (loadArgs->chunkBufferSize == 0x400000) {
      error = LiRefillUpcomingBounceBuffer(rplBasics, 2);
   } else {
      LiInitBuffer(false);
   }

   if (error != 0 && allocPtr) {
      LiCacheLineCorrectFreeEx(loadArgs->readHeapTracking, allocPtr, allocSize);
   }

   return error;
}

static virt_ptr<rpl::SectionHeader>
getSectionHeader(virt_ptr<RPLBasics> rplBasics,
                 virt_ptr<rpl::SectionHeader> sectionHeaderBuffer,
                 uint32_t idx)
{
   auto base = virt_cast<virt_addr>(sectionHeaderBuffer);
   auto offset = idx * rplBasics->elfHeader.shentsize;
   return virt_cast<rpl::SectionHeader *>(base + offset);
}

static virt_ptr<rpl::SectionHeader>
getSectionHeader(virt_ptr<RPLBasics> rplBasics,
                 uint32_t idx)
{
   return getSectionHeader(rplBasics,
                           virt_cast<rpl::SectionHeader *>(rplBasics->sectionHeaderBuffer),
                           idx);
}

int32_t
LiCheckFileBounds(virt_ptr<RPLBasics> rplBasics)
{
   auto shBase = virt_cast<virt_addr>(rplBasics->sectionHeaderBuffer);
   auto dataMin = -1;
   auto dataMax = 0;

   auto readMin = -1;
   auto readMax = 0;

   auto textMin = -1;
   auto textMax = 0;

   auto tempMin = -1;
   auto tempMax = 0;

   for (auto i = 0u; i < rplBasics->elfHeader.shnum; ++i) {
      auto sectionHeader = virt_cast<rpl::SectionHeader *>(shBase + i * rplBasics->elfHeader.shentsize);
      if (sectionHeader->size == 0 ||
          sectionHeader->type == rpl::SHT_RPL_FILEINFO ||
          sectionHeader->type == rpl::SHT_RPL_CRCS ||
          sectionHeader->type == rpl::SHT_NOBITS ||
          sectionHeader->type == rpl::SHT_RPL_IMPORTS) {
         continue;
      }

      if ((sectionHeader->flags & rpl::SHF_EXECINSTR) &&
          sectionHeader->type != rpl::SHT_RPL_EXPORTS) {
         textMin = std::min(textMin, static_cast<int32_t>(sectionHeader->offset));
         textMax = std::min(textMax, static_cast<int32_t>(sectionHeader->offset + sectionHeader->size));
      } else {
         if (sectionHeader->flags & rpl::SHF_ALLOC) {
            if (sectionHeader->flags & rpl::SHF_WRITE) {
               dataMin = std::min(dataMin, static_cast<int32_t>(sectionHeader->offset));
               dataMax = std::min(dataMax, static_cast<int32_t>(sectionHeader->offset + sectionHeader->size));
            } else {
               readMin = std::min(readMin, static_cast<int32_t>(sectionHeader->offset));
               readMax = std::min(readMax, static_cast<int32_t>(sectionHeader->offset + sectionHeader->size));
            }
         } else {
            tempMin = std::min(tempMin, static_cast<int32_t>(sectionHeader->offset));
            tempMax = std::min(tempMax, static_cast<int32_t>(sectionHeader->offset + sectionHeader->size));
         }
      }
   }

   if (dataMin == -1) {
      dataMax = (rplBasics->elfHeader.shnum * rplBasics->elfHeader.shentsize) + rplBasics->elfHeader.shoff;
   }

   if (readMin == -1) {
      readMin = dataMax;
      readMax = dataMax;
   }

   if (textMin == -1) {
      textMin = readMax;
      textMax = readMax;
   }

   if (tempMin == -1) {
      tempMin = textMax;
      tempMax = textMax;
   }

   if (dataMin < rplBasics->elfHeader.shoff) {
      Loader_ReportError("*** SecHrs, FileInfo, or CRCs in bad spot in file. Return %d.\n", -470026);
      goto error;
   }

   // Data
   if (dataMin > dataMax) {
      Loader_ReportError("*** DataMin > DataMax. break.\n");
      goto error;
   }

   if (dataMin > readMin) {
      Loader_ReportError("*** DataMin > ReadMin. break.\n");
      goto error;
   }

   if (dataMax > readMin) {
      Loader_ReportError("*** DataMax > ReadMin. break.\n");
      goto error;
   }

   // Read
   if (readMin > readMax) {
      Loader_ReportError("*** ReadMin > ReadMax. break.\n");
      goto error;
   }

   if (readMin > textMin) {
      Loader_ReportError("*** ReadMin > TextMin. break.\n");
      goto error;
   }

   if (readMax > textMin) {
      Loader_ReportError("*** ReadMax > TextMin. break.\n");
      goto error;
   }

   // Text
   if (textMin > textMax) {
      Loader_ReportError("*** TextMin > TextMax. break.\n");
      goto error;
   }

   if (textMin > tempMin) {
      Loader_ReportError("*** TextMin > TempMin. break.\n");
      goto error;
   }

   if (textMax > tempMin) {
      Loader_ReportError("*** TextMax > TempMin. break.\n");
      goto error;
   }

   // Temp
   if (tempMin > tempMax) {
      Loader_ReportError("*** TempMin > TempMax. break.\n");
      goto error;
   }

   return 0;

error:
   LiSetFatalError(0x18729B, rplBasics->fileType, 1, "LiCheckFileBounds", 0x247);
   return -470026;
}

uint32_t
LiCalcCRC32(uint32_t crc,
            const virt_ptr<void> data,
            uint32_t size)
{
   LiCheckAndHandleInterrupts();
   if (!data || !size) {
      return crc;
   }

   crc = crc32(crc, reinterpret_cast<Bytef *>(data.getRawPointer()), size);
   LiCheckAndHandleInterrupts();
   return crc;
}

int32_t
LiLoadRPLBasics(virt_ptr<char> moduleName,
                uint32_t moduleNameLen,
                virt_ptr<void> chunkBuffer,
                virt_ptr<TinyHeap> codeHeapTracking,
                virt_ptr<TinyHeap> dataHeapTracking,
                uint32_t r8,
                uint32_t r9,
                virt_ptr<RPLBasics> *outRplBasics,
                RPLBasicsLoadArgs *loadArgs,
                uint32_t arg_C)
{
   struct LoadAttemptErrorData
   {
      int32_t error;
      ios::mcp::MCPFileType fileType;
      uint32_t fatalErr;
      virt_ptr<char> fatalFunction;
      uint32_t fatalLine;
      uint32_t fatalMsgType;
   };

   LoadAttemptErrorData loadAttemptErrors[3];
   int32_t loadAttempt = 0;
   uint32_t chunkReadSize;
   int32_t error;

   while (true) {
      error = LiWaitOneChunk(&chunkReadSize, loadArgs->pathName.getRawPointer(), loadArgs->fileType);
      if (error == 0) {
         break;
      }

      if (loadAttempt < 2) {
         if (sData->gFatalErr) {
            auto &attemptErrors = loadAttemptErrors[loadAttempt];
            attemptErrors.error = error;
            attemptErrors.fileType = loadArgs->fileType;
            attemptErrors.fatalErr = sData->gFatalErr;
            attemptErrors.fatalFunction = sData->gFatalFunction;
            attemptErrors.fatalLine = sData->gFatalLine;
            attemptErrors.fatalMsgType = sData->gFatalMsgType;
            LiResetFatalError();
         }

         if (loadAttempt == 0) {
            if (loadArgs->fileType == ios::mcp::MCPFileType::ProcessCode) {
               loadArgs->fileType = ios::mcp::MCPFileType::CafeOS;
            }
         } else {
            loadArgs->fileType = ios::mcp::MCPFileType::SystemDataCode;
         }

         LiInitBuffer(false);
         chunkReadSize = 0;
         loadArgs->chunkBufferSize = 0;
         error = LiBounceOneChunk(loadArgs->pathName.getRawPointer(),
                                  loadArgs->fileType,
                                  loadArgs->upid,
                                  virt_addrof(loadArgs->chunkBufferSize),
                                  loadArgs->fileOffset,
                                  1,
                                  virt_addrof(loadArgs->chunkBuffer));
      }

      if (error != 0) {
         Loader_ReportError("***Loader failure %d first time, %d second time and %d third time. loading \"%s\".\n",
                            loadAttemptErrors[0].error,
                            loadAttemptErrors[1].error,
                            error,
                            loadArgs->pathName.getRawPointer());
         return error;
      }
   }

   LiCheckAndHandleInterrupts();

   // Load and validate the ELF header
   uint32_t filePhStride;
   uint32_t fileShStride;
   virt_ptr<rpl::Header> fileElfHeader;
   virt_ptr<rpl::SectionHeader> fileSectionHeaders;
   error = ELFFILE_ValidateAndPrepareMinELF(chunkBuffer,
                                            chunkReadSize,
                                            &fileElfHeader,
                                            &fileSectionHeaders,
                                            &fileShStride,
                                            &filePhStride);
   if (error) {
      Loader_ReportError("*** Failed ELF file checks (err=0x%08X)\n", error);
      LiSetFatalError(0x18729B, loadArgs->fileType, 1, "LiLoadRPLBasics", 0x325);
      return error;
   }

   // Check that this ELF looks like a Wii U Cafe RPL
   if (fileElfHeader->fileClass != rpl::ELFCLASS32 ||
       fileElfHeader->encoding != rpl::ELFDATA2MSB ||
       fileElfHeader->abi != rpl::EABI_CAFE ||
       fileElfHeader->elfVersion > rpl::EV_CURRENT ||
       fileElfHeader->machine != rpl::EM_PPC ||
       fileElfHeader->version != 1 ||
       fileElfHeader->shnum < 2) {
      return -470025;
   }

   // Initialise temporary RPL basics
   cafe::StackObject<RPLBasics> tmpRplBasics;
   virt_ptr<RPLBasics> rplBasics;
   rplBasics = tmpRplBasics;

   std::memset(rplBasics.getRawPointer(), 0, sizeof(RPLBasics));
   if (r9 == 0) {
      rplBasics->upid0x00 = sData->upid;
   }

   std::memcpy(virt_addrof(rplBasics->elfHeader).getRawPointer(),
               fileElfHeader.getRawPointer(),
               sizeof(rpl::Header));

   // Check some offsets are valid
   if (!rplBasics->elfHeader.shentsize) {
      rplBasics->elfHeader.shentsize = sizeof(rpl::SectionHeader);
   }

   if (rplBasics->elfHeader.shoff >= chunkReadSize ||
       ((rplBasics->elfHeader.shnum - 1) * rplBasics->elfHeader.shentsize) + rplBasics->elfHeader.shoff >= chunkReadSize) {
      LiSetFatalError(0x18729B, loadArgs->fileType, 1, "LiLoadRPLBasics", 0x33F);
      return -470077;
   }

   auto shRplCrcs = getSectionHeader(rplBasics, fileSectionHeaders, rplBasics->elfHeader.shnum - 2);
   if (shRplCrcs->offset >= chunkReadSize ||
       shRplCrcs->offset + shRplCrcs->size >= chunkReadSize) {
      LiSetFatalError(0x18729B, loadArgs->fileType, 1, "LiLoadRPLBasics", 0x348);
      return -470077;
   }

   auto shRplFileInfo = getSectionHeader(rplBasics, fileSectionHeaders, rplBasics->elfHeader.shnum - 1);
   if (shRplFileInfo->offset + shRplFileInfo->size >= chunkReadSize) {
      LiSetFatalError(0x18729B, loadArgs->fileType, 1, "LiLoadRPLBasics", 0x351);
      return -470078;
   }

   // Check RPL file info
   if (shRplFileInfo->type != rpl::SHT_RPL_FILEINFO ||
       shRplFileInfo->flags & rpl::SHF_DEFLATED) {
      Loader_ReportError("***shnum-1 section type = 0x%08X, flags=0x%08X\n", shRplFileInfo->type, shRplFileInfo->flags);
      LiSetFatalError(0x18729B, loadArgs->fileType, 1, "LiLoadRPLBasics", 0x35A);
      return -470082;
   }

   auto rplFileInfo = virt_cast<rpl::RPLFileInfo_v4_2 *>(virt_cast<virt_addr>(fileElfHeader) + shRplFileInfo->offset);
   if (rplFileInfo->version < rpl::RPLFileInfo_v4_2::Version) {
      Loader_ReportError("*** COS requires that %.*s be built with at least SDK %d.%02d.%d, it was built with an older SDK\n",
                         moduleName, moduleNameLen,
                         2, 5, 0);
      LiSetFatalError(0x187298, loadArgs->fileType, 1, "LiLoadRPLBasics", 0x38B);
      return -470062;
   }

   if (rplFileInfo->minVersion < 0x5014 || rplFileInfo->minVersion > 0x5335) {
      auto major = rplFileInfo->minVersion / 10000;
      auto minor = (rplFileInfo->minVersion % 10000) / 100;
      auto patch = rplFileInfo->minVersion % 100;
      Loader_ReportError("*** COS requires that %.*s be built with at least SDK %d.%02d.%d, it was built with SDK %d.%02d.%d\n",
                         moduleName, moduleNameLen,
                         2, 5, 0,
                         major, minor, patch);
      LiSetFatalError(0x187298, loadArgs->fileType, 1, "LiLoadRPLBasics", 0x38B);
      return -470062;
   }

   uint32_t largestFree;
   if (rplFileInfo->textSize) {
      error = LiCacheLineCorrectAllocEx(codeHeapTracking,
                                        rplFileInfo->textSize,
                                        rplFileInfo->textAlign,
                                        virt_addrof(rplBasics->textBuffer),
                                        0,
                                        virt_addrof(rplBasics->textBufferSize),
                                        &largestFree,
                                        loadArgs->fileType);
      if (error != 0) {
         Loader_ReportError("***Could not allocate uncompressed text (%d) in %s heap \"%.*s\";  (needed %d, available %d).\n",
                            rplFileInfo->textSize,
                            r9 ? "shared code" : "code",
                            moduleName, moduleNameLen,
                            rplFileInfo->textSize,
                            largestFree);
         goto lblError;
      }
   }

   virt_ptr<void> allocPtr;
   uint32_t rplBasicAllocSize;
   error = LiCacheLineCorrectAllocEx(dataHeapTracking,
                                     sizeof(RPLBasics),
                                     4,
                                     &allocPtr,
                                     0,
                                     &rplBasicAllocSize,
                                     &largestFree,
                                     loadArgs->fileType);
   if (error != 0) {
      Loader_ReportError("*** Failed %s alloc (err=0x%08X);  (needed %d, available %d)\n",
                         r9 ? "readheap" : "workarea",
                         error,
                         sizeof(RPLBasics),
                         largestFree);
      goto lblError;
   }

   rplBasics = virt_cast<RPLBasics *>(allocPtr);
   std::memcpy(rplBasics.getRawPointer(), &tmpRplBasics, sizeof(RPLBasics));

   rplBasics->selfBufferSize = rplBasicAllocSize;
   loadArgs->rplBasics = rplBasics;

   error = LiInitBufferTracking(loadArgs);
   if (error != 0) {
      goto lblError;
   }

   uint32_t largestFree;
   error = LiCacheLineCorrectAllocEx(dataHeapTracking,
                                     rplBasics->elfHeader.shnum * 8,
                                     4,
                                     virt_addrof(rplBasics->sectionAddressBuffer),
                                     1,
                                     virt_addrof(rplBasics->sectionAddressBufferSize),
                                     &largestFree,
                                     rplBasics->fileType);
   if (error != 0) {
      Loader_ReportError("***Allocate Error %d, Failed to allocate %d bytes for section addresses;  (needed %d, available %d).\n",
                         error,
                         rplBasics->elfHeader.shnum * 8,
                         rplBasics->sectionAddressBufferSize,
                         largestFree);
      goto lblError;
   }

   auto sectionAddresses = virt_cast<virt_addr *>(rplBasics->sectionAddressBuffer);

   error = LiCacheLineCorrectAllocEx(dataHeapTracking,
                                     rplBasics->elfHeader.shnum * rplBasics->elfHeader.shentsize,
                                     4,
                                     virt_addrof(rplBasics->sectionHeaderBuffer),
                                     1,
                                     virt_addrof(rplBasics->sectionHeaderBufferSize),
                                     &largestFree,
                                     rplBasics->fileType);
   if (error != 0) {
      Loader_ReportError("*** Could not allocate space for section headers in %s heap;  (needed %d, available %d)\n",
                         r9 ? "shared readonly" : "readonly",
                         rplBasics->sectionHeaderBufferSize,
                         largestFree);
      goto lblError;
   }

   std::memcpy(rplBasics->sectionHeaderBuffer.getRawPointer(),
               virt_cast<void *>(virt_cast<virt_addr>(fileElfHeader) + rplBasics->elfHeader.shoff).getRawPointer(),
               rplBasics->elfHeader.shnum * rplBasics->elfHeader.shentsize);

   if (r8) {
      error = LiCacheLineCorrectAllocEx(sData->codeAreaTracking,
                                        align_up(moduleNameLen, 4),
                                        4,
                                        virt_addrof(rplBasics->moduleNameBuffer),
                                        1,
                                        virt_addrof(rplBasics->moduleNameBufferSize),
                                        &largestFree,
                                        rplBasics->fileType);
      if (error != 0) {
         Loader_ReportError("*** Could not allocate space for module name;  (needed %d, available %d)\n",
                            rplBasics->moduleNameBufferSize,
                            largestFree);
         goto lblError;
      }

      std::strncpy(virt_cast<char *>(rplBasics->moduleNameBuffer).getRawPointer(),
                   moduleName.getRawPointer(),
                   rplBasics->moduleNameBufferSize);

      moduleNameLen = rplBasics->moduleNameBufferSize;
   } else {
      rplBasics->moduleNameBuffer = moduleName;
      rplBasics->moduleNameBufferSize = moduleNameLen;
   }

   rplBasics->moduleNameLen = moduleNameLen;

   // Load SHT_RPL_CRCS
   shRplCrcs = getSectionHeader(rplBasics, rplBasics->elfHeader.shnum - 2);
   if (shRplCrcs->type != rpl::SHT_RPL_CRCS ||
       (shRplCrcs->flags & rpl::SHF_DEFLATED)) {
      Loader_ReportError("***shnum-2 section type = 0x%08X, flags=0x%08X\n",
                         shRplCrcs->type, shRplCrcs->flags);
      LiSetFatalError(0x18729B, loadArgs->fileType, 1, "LiLoadRPLBasics", 0x403);
      error = -470081;
      goto lblError;
   }

   error = LiCacheLineCorrectAllocEx(sData->codeAreaTracking,
                                     shRplCrcs->size,
                                     -shRplCrcs->addralign,
                                     virt_addrof(rplBasics->rplCrcBuffer),
                                     1,
                                     virt_addrof(rplBasics->rplCrcBufferSize),
                                     &largestFree,
                                     rplBasics->fileType);
   if (error != 0) {
      Loader_ReportError("*** Could not allocate space for CRCs;  (needed %d, available %d)\n",
                         rplBasics->rplCrcBufferSize, largestFree);
      goto lblError;
   }

   auto sectionCrcs = virt_cast<uint32_t *>(rplBasics->rplCrcBuffer);
   std::memcpy(rplBasics->rplCrcBuffer.getRawPointer(),
               virt_cast<void *>(virt_cast<virt_addr>(fileElfHeader) + shRplCrcs->offset).getRawPointer(),
               shRplCrcs->size);

   sectionAddresses[rplBasics->elfHeader.shnum - 2] = virt_cast<virt_addr>(rplBasics->rplCrcBuffer);

   // Load SHT_RPL_FILEINFO
   shRplFileInfo = getSectionHeader(rplBasics, rplBasics->elfHeader.shnum - 1);
   if (shRplFileInfo->type != rpl::SHT_RPL_FILEINFO ||
      (shRplFileInfo->flags & rpl::SHF_DEFLATED)) {
      Loader_ReportError("***shnum-1 section type = 0x%08X, flags=0x%08X\n",
                         shRplFileInfo->type, shRplFileInfo->flags);
      LiSetFatalError(0x18729B, loadArgs->fileType, 1, "LiLoadRPLBasics", 0x41A);
      error = -470082;
      goto lblError;
   }

   rplBasics->rplFileInfoSize = shRplFileInfo->size;
   error = LiCacheLineCorrectAllocEx(dataHeapTracking,
                                     shRplFileInfo->size,
                                     shRplFileInfo->addralign,
                                     virt_addrof(rplBasics->rplFileInfoBuffer),
                                     1,
                                     virt_addrof(rplBasics->rplFileInfoBufferSize),
                                     &largestFree,
                                     rplBasics->fileType);
   if (error != 0) {
      Loader_ReportError("*** Could not allocate space for file info;  (needed %d, available %d)\n",
                         rplBasics->rplFileInfoBufferSize, largestFree);
      goto lblError;
   }

   if ((sData->gProcFlags >> 9) & 7) {
      Loader_ReportInfo("RPL_LAYOUT:%s,FILE,start,=\"0%08x\"\nRPL_LAYOUT:%s,FILE,end,=\"0%08x\"\n",
                        rplBasics->moduleNameBuffer,
                        rplBasics->rplFileInfoBuffer,
                        rplBasics->moduleNameBuffer,
                        virt_cast<virt_addr>(rplBasics->rplFileInfoBuffer) + rplBasics->rplFileInfoBufferSize);
   }

   auto rplFileInfo = virt_cast<rpl::RPLFileInfo_v4_2 *>(rplBasics->rplFileInfoBuffer);
   std::memcpy(rplBasics->rplFileInfoBuffer.getRawPointer(),
               virt_cast<void *>(virt_cast<virt_addr>(fileElfHeader) + shRplFileInfo->offset).getRawPointer(),
               shRplFileInfo->size);

   sectionAddresses[rplBasics->elfHeader.shnum - 1] = virt_cast<virt_addr>(rplBasics->rplFileInfoBuffer);

   auto rplFileInfoCrc = LiCalcCRC32(0, rplBasics->rplFileInfoBuffer, shRplFileInfo->size);
   if (rplFileInfoCrc != sectionCrcs[rplBasics->elfHeader.shnum - 1]) {
      Loader_ReportError("***FileInfo CRC failed check.\n");
      LiSetFatalError(0x18729B, loadArgs->fileType, 1, "LiLoadRPLBasics", 0x433);
      error = -470083;
      goto lblError;
   }

   rplFileInfo->tlsModuleIndex = -1;
   error = LiCheckFileBounds(rplBasics);
   if (error == 0) {
      rplBasics->firstChunkElfHeader = fileElfHeader;
      *outRplBasics = rplBasics;
      return error;
   }

lblError:
   if (rplBasics) {
      if (rplBasics->rplFileInfoBuffer) {
         LiCacheLineCorrectFreeEx(dataHeapTracking,
                                  rplBasics->rplFileInfoBuffer,
                                  rplBasics->rplFileInfoBufferSize);
      }

      if (rplBasics->rplCrcBuffer) {
         LiCacheLineCorrectFreeEx(sData->codeAreaTracking,
                                  rplBasics->rplCrcBuffer,
                                  rplBasics->rplCrcBufferSize);
      }

      if (rplBasics->moduleNameBuffer) {
         LiCacheLineCorrectFreeEx(sData->codeAreaTracking,
                                  rplBasics->moduleNameBuffer,
                                  rplBasics->moduleNameBufferSize);
      }

      if (rplBasics->sectionHeaderBuffer) {
         LiCacheLineCorrectFreeEx(dataHeapTracking,
                                  rplBasics->sectionHeaderBuffer,
                                  rplBasics->sectionHeaderBufferSize);
      }

      if (rplBasics->sectionAddressBuffer) {
         LiCacheLineCorrectFreeEx(dataHeapTracking,
                                  rplBasics->sectionAddressBuffer,
                                  rplBasics->sectionAddressBufferSize);
      }

      if (rplBasics->pathBuffer) {
         LiCacheLineCorrectFreeEx(dataHeapTracking,
                                  rplBasics->pathBuffer,
                                  rplBasics->pathBufferSize);
      }

      if (rplBasics->textBuffer) {
         LiCacheLineCorrectFreeEx(codeHeapTracking,
                                  rplBasics->textBuffer,
                                  rplBasics->textBufferSize);
      }

      LiCacheLineCorrectFreeEx(codeHeapTracking,
                               rplBasics,
                               rplBasics->selfBufferSize);
   }

   return error;
}

struct SegmentBounds
{
   int32_t min = -1;
   int32_t max = 0;
   uint32_t unk_0x08 = 0;
   const char *name = nullptr;
};

struct RplSegmentBounds
{
   SegmentBounds data;
   SegmentBounds load;
   SegmentBounds text;
   SegmentBounds temp;
};

void
LiClearUserBss(uint32_t unk_r3,
               kernel::UniqueProcessId upid,
               virt_addr address,
               uint32_t size)
{
   if (unk_r3 == 0) {
      if (upid == kernel::UniqueProcessId::HomeMenu ||
          upid == kernel::UniqueProcessId::OverlayMenu ||
          upid == kernel::UniqueProcessId::ErrorDisplay ||
          upid == kernel::UniqueProcessId::Game) {
         return;
      }
   }

   std::memset(virt_cast<void *>(address).getRawPointer(), 0, size);
}

int32_t
GetNextBounce(virt_ptr<RPLBasics> rplBasics)
{
   uint32_t chunkBytesRead = 0;
   auto error = LiWaitOneChunk(&chunkBytesRead,
                               virt_cast<char *>(rplBasics->pathBuffer).getRawPointer(),
                               rplBasics->fileType);
   if (error != 0) {
      return error;
   }

   auto upcomingBufferNumber = rplBasics->upcomingBufferNumber;
   if (upcomingBufferNumber == 1) {
      rplBasics->upcomingBufferNumber = 2;
      rplBasics->fileReadSize += chunkBytesRead;
      rplBasics->firstChunkBuffer = rplBasics->chunkBuffer;
      rplBasics->fileOffset = rplBasics->upcomingFileOffset + chunkBytesRead;
      rplBasics->unk0x90 += chunkBytesRead;
      rplBasics->upcomingFileOffset += chunkBytesRead;
   } else {
      rplBasics->fileOffset = rplBasics->upcomingFileOffset + chunkBytesRead;
      rplBasics->upcomingFileOffset += chunkBytesRead;
      rplBasics->firstChunkBuffer = rplBasics->chunkBuffer;
      rplBasics->firstChunkElfHeader = rplBasics->firstChunkElfHeader - rplBasics->fileReadSize;
      rplBasics->fileReadSize = chunkBytesRead; // wat face
      rplBasics->unk0x90 += chunkBytesRead;
      rplBasics->upcomingBufferNumber = 1;
   }

   // ^^^ Basically all these var names are FUCKING WRONG... shit.

   if (chunkBytesRead == 0x400000) {
      return LiRefillUpcomingBounceBuffer(rplBasics, upcomingBufferNumber);
   } else {
      LiInitBuffer(false);
      return 0;
   }
}

int32_t
sLiPrepareBounceBufferForReading(virt_ptr<RPLBasics> rplBasics,
                                 uint32_t shndx,
                                 std::string_view name,
                                 uint32_t sectionOffset,
                                 uint32_t *outUnkR7,
                                 uint32_t sectionSize,
                                 uint32_t *outUnkR9)
{
   LiCheckAndHandleInterrupts();

   // r21 = -470076
   // r22 = 0x18729B
   if (rplBasics->upcomingFileOffset <= rplBasics->fileOffset) {
      if (shndx == 0) {
         Loader_ReportError("*** %s Segment %s: bounce buffer has no size or is corrupted.\n",
                            rplBasics->moduleNameBuffer, name);
      } else {
         Loader_ReportError("*** %s Segment %s's segment %d: bounce buffer has no size or is corrupted.\n",
                            rplBasics->moduleNameBuffer, name, shndx);
      }

      LiSetFatalError(0x18729B, rplBasics->fileType, 1, "sLiPrepareBounceBufferForReading", 0xAD);
      return -470074;
   }

   while (sectionOffset >= rplBasics->upcomingFileOffset) {
      auto error = GetNextBounce(rplBasics);
      if (error != 0) {
         break;
      }
   }

   if (sectionOffset < rplBasics->fileOffset) {
      // TODO: Finish sLiPrepareBounceBufferForReading
   }
}

int32_t
sLiSetupOneAllocSection(kernel::UniqueProcessId upid,
                        virt_ptr<RPLBasics> rplBasics,
                        uint32_t shndx,
                        virt_ptr<rpl::SectionHeader> sectionHeader,
                        uint32_t unk_r7,
                        RplSegmentBounds *bounds,
                        uint32_t unk_r9,
                        uint32_t rplDataAlign,
                        uint32_t unk_arg8)
{
   LiCheckAndHandleInterrupts();

   auto sectionAddresses = virt_cast<virt_addr *>(rplBasics->sectionAddressBuffer);
   auto sectionAddress = virt_addr { (sectionHeader->addr - bounds->data.min) + unk_r9 };
   sectionAddresses[shndx] = sectionAddress;

   auto fileData = virt_cast<void *>(virt_cast<virt_addr>(rplBasics->firstChunkElfHeader) + sectionHeader->offset);

   // r24 = -470091
   // r25 = 0x18729B
   if (!align_check(sectionAddress, sectionHeader->addralign)) {
      Loader_ReportError("***%s section %d alignment failure.\n",
                         bounds->data.name, shndx);
      Loader_ReportError("Ptr              = 0x%08X\n", sectionAddress);
      Loader_ReportError("%s base        = 0x%08X\n", bounds->data.name, unk_r9);
      Loader_ReportError("%s base align  = %d\n", bounds->data.name, rplDataAlign);
      Loader_ReportError("SecHdr->addr     = 0x%08X\n", sectionHeader->addr);
      Loader_ReportError("bound[%s].base = 0x%08X\n", bounds->data.name, bounds->data.min);
      LiSetFatalError(0x18729B, rplBasics->fileType, 1, "sLiSetupOneAllocSection", 0x5E6);
      return -470091 + 0x30;
   }

   if (unk_arg8 == 0 &&
       sectionHeader->type == rpl::SHT_NOBITS &&
       unk_r7) {
      auto userControl = true;
      if (upid != kernel::UniqueProcessId::Invalid) {
         userControl = false;// *0xEFE01024;
      }

      LiClearUserBss(userControl, upid, sectionAddress, sectionHeader->size);
      if (bounds->data.unk_0x08 < sectionHeader->addr + sectionHeader->size) {
         bounds->data.unk_0x08 = sectionHeader->addr + sectionHeader->size;
      }
   } else {
      uint32_t outUnkR7, outUnkR9;
      sLiPrepareBounceBufferForReading(rplBasics, shndx, bounds->data.name, sectionHeader->offset, &outUnkR7, sectionHeader->size, &outUnkR9);
   }

   if (bounds->data.unk_0x08 > bounds->data.max) {
      Loader_ReportError("*** %s section %d segment %s makerpl's segment size was wrong: real time calculated size =0x%08X makerpl's size=0x%08X.\n",
                         rplBasics->moduleNameBuffer, shndx, bounds->data.name,
                         bounds->data.unk_0x08, bounds->data.max);
      LiSetFatalError(0x18729B, rplBasics->fileType, 1, "sLiSetupOneAllocSection", 0x68C);
      return -470091;
   }

   return 0;
}

int32_t
LiSetupOneRPL(kernel::UniqueProcessId upid,
              virt_ptr<RPLBasics> rplBasics,
              virt_ptr<TinyHeap> codeHeapTracking,
              virt_ptr<TinyHeap> dataHeapTracking)
{
   auto shBase = virt_cast<virt_addr>(rplBasics->sectionHeaderBuffer);

   // Initialise stack 4x -1, 0, 0, 0

   // r25+0x0C "DATA"
   // r25+0x1C "LOADERINFO"
   // r25+0x2C "TEXT"
   // r25+0x3C "TEMP"
   RplSegmentBounds bounds;
   bounds.data.name = "DATA";
   bounds.load.name = "LOADERINFO";
   bounds.text.name = "TEXT";
   bounds.temp.name = "TEMP";

   for (auto i = 0u; i < rplBasics->elfHeader.shnum; ++i) {
      auto sectionHeader = virt_cast<rpl::SectionHeader *>(shBase + i * rplBasics->elfHeader.shentsize);
      if (sectionHeader->size == 0 ||
          sectionHeader->type == rpl::SHT_RPL_FILEINFO ||
          sectionHeader->type == rpl::SHT_RPL_CRCS ||
          sectionHeader->type == rpl::SHT_RPL_IMPORTS) {
         continue;
      }

      if (sectionHeader->flags & rpl::SHF_ALLOC) {
         if ((sectionHeader->flags & rpl::SHF_EXECINSTR) &&
             sectionHeader->type != rpl::SHT_RPL_EXPORTS) {
            bounds.text.min = std::min(bounds.text.min, static_cast<int32_t>(sectionHeader->addr));
         } else {
            if (sectionHeader->flags & rpl::SHF_WRITE) {
               bounds.data.min = std::min(bounds.data.min, static_cast<int32_t>(sectionHeader->addr));
            } else {
               bounds.load.min = std::min(bounds.load.min, static_cast<int32_t>(sectionHeader->addr));
            }
         }
      } else {
         bounds.temp.min = std::min(bounds.temp.min, static_cast<int32_t>(sectionHeader->addr));
         bounds.temp.max = std::max(bounds.temp.max, static_cast<int32_t>(sectionHeader->addr + sectionHeader->size));
      }
   }

   if (bounds.data.min == -1) {
      bounds.data.min = 0;
   }

   if (bounds.load.min == -1) {
      bounds.load.min = 0;
   }

   if (bounds.text.min == -1) {
      bounds.text.min = 0;
   }

   if (bounds.temp.min == -1) {
      bounds.temp.min = 0;
   }

   auto rplFileInfo = virt_cast<rpl::RPLFileInfo_v4_2 *>(rplBasics->rplFileInfoBuffer);
   bounds.text.max = (bounds.text.min + rplFileInfo->textSize) - rplFileInfo->trampAdjust;
   bounds.data.max = bounds.data.min + rplFileInfo->dataSize;
   bounds.load.max = (bounds.load.min + rplFileInfo->loadSize) - rplFileInfo->fileInfoPad;

   auto textSize = bounds.text.max - bounds.text.min;
   auto dataSize = bounds.data.max - bounds.data.min;
   auto loadSize = bounds.load.max - bounds.load.min;
   auto tempSize = bounds.temp.max - bounds.temp.min;

   // r26 = 0xFFF8D3B5

   if (rplFileInfo->trampAdjust >= textSize ||
       rplFileInfo->textSize - rplFileInfo->trampAdjust < textSize ||
       rplFileInfo->dataSize < dataSize ||
       rplFileInfo->loadSize - rplFileInfo->fileInfoPad < loadSize ||
       rplFileInfo->tempSize < tempSize) {
      Loader_ReportError("***Bounds check failure.\n");
      Loader_ReportError("b%d: %08X %08x\n", 0, bounds.data.min, bounds.data.max);
      Loader_ReportError("b%d: %08X %08x\n", 1, bounds.load.min, bounds.load.max);
      Loader_ReportError("b%d: %08X %08x\n", 2, bounds.text.min, bounds.text.max);
      Loader_ReportError("b%d: %08X %08x\n", 3, bounds.temp.min, bounds.temp.max);
      Loader_ReportError("TrampAdj = %08X\n", rplFileInfo->trampAdjust);
      Loader_ReportError("Text = %08X\n", rplFileInfo->textSize);
      Loader_ReportError("Data = %08X\n", rplFileInfo->dataSize);
      Loader_ReportError("Read = %08X\n", rplFileInfo->loadSize - rplFileInfo->fileInfoPad);
      Loader_ReportError("Temp = %08X\n", rplFileInfo->tempSize);
      LiSetFatalError(0x18729B, rplBasics->fileType, 1, "LiSetupOneRPL", 0x715);

      // TODO: Free and shit
      return -470091 + 0x31;
   }

   auto unk_var_84 = 0;
   auto sectionAddresses = virt_cast<virt_addr *>(rplBasics->sectionAddressBuffer);

   if (rplBasics->unk0xBC) {
      for (auto i = 0u; i < rplBasics->elfHeader.shnum; ++i) {
         LiCheckAndHandleInterrupts();

         auto sectionHeader = virt_cast<rpl::SectionHeader *>(shBase + i * rplBasics->elfHeader.shentsize);
         if (sectionHeader->size == 0 ||
             sectionAddresses[i] ||
             (sectionHeader->flags & rpl::SHF_WRITE) ||
             (sectionHeader->flags & rpl::SHF_ALLOC)) {
            continue;
         }

         sLiSetupOneAllocSection(upid, rplBasics, i, sectionHeader, 1, &bounds, unk_var_84, rplFileInfo->dataAlign, 0);
      }
   }
}

int32_t
sLoadOneShared(std::string_view pathName)
{
   StackObject<uint32_t> chunkSize;
   StackObject<virt_ptr<void>> chunkBuffer;
   StackArray<char, 32> pathNameBuffer;
   virt_ptr<RPLBasics> rplBasics;
   RPLBasicsLoadArgs rplBasicLoadArgs;

   LiInitBuffer(false);

   auto error = LiBounceOneChunk(pathName,
                                 ios::mcp::MCPFileType::CafeOS,
                                 kernel::UniqueProcessId::Kernel,
                                 chunkSize,
                                 0,
                                 1,
                                 chunkBuffer);
   if (error != 0) {
      return error;
   }

   auto moduleName = virt_ptr<char> { pathNameBuffer };
   auto moduleNameLen = std::min<uint32_t>(pathName.size(), 0x1Fu);

   std::memcpy(pathNameBuffer.getRawPointer(),
               pathName.data(),
               moduleNameLen);
   moduleName[moduleNameLen] = 0;

   LiResolveModuleName(&moduleName, &moduleNameLen);

   rplBasicLoadArgs.fileOffset = 0;
   rplBasicLoadArgs.pathNameLen = pathName.size();
   rplBasicLoadArgs.pathName = pathNameBuffer;
   rplBasicLoadArgs.chunkBuffer = chunkBuffer;
   rplBasicLoadArgs.chunkBufferSize = *chunkSize;
   rplBasicLoadArgs.readHeapTracking = sData->gpSharedReadHeapTracking;
   rplBasicLoadArgs.upid = kernel::UniqueProcessId::Kernel;
   rplBasicLoadArgs.fileType = ios::mcp::MCPFileType::CafeOS;

   error = LiLoadRPLBasics(moduleName,
                           moduleNameLen,
                           chunkBuffer,
                           sData->gpSharedCodeHeapTracking,
                           sData->gpSharedReadHeapTracking,
                           0,
                           1,
                           &rplBasics,
                           &rplBasicLoadArgs,
                           0);
   if (error != 0) {
      return error;
   }

   rplBasics->unk0x70 |= 0x20000004;

   auto rplFileInfo = virt_cast<rpl::RPLFileInfo_v4_2 *>(rplBasics->rplFileInfoBuffer);
   if (rplFileInfo->dataSize) {
      rplBasics->unk0xBC = align_up(sData->gpLoaderShared->unk0x04, rplFileInfo->dataAlign);
      sData->gpLoaderShared->unk0x04 = align_up(rplBasics->unk0xBC, 64);
   }

   if (rplFileInfo->loadSize != rplFileInfo->fileInfoPad) {
      auto tinyHeapError = TinyHeap_Alloc(sData->gpSharedReadHeapTracking,
                                          rplFileInfo->loadSize - rplFileInfo->fileInfoPad,
                                          -rplFileInfo->loadAlign,
                                          virt_addrof(rplBasics->sharedLibraryLoadBuffer));
      if (tinyHeapError != TinyHeapError::OK) {
         Loader_ReportError("Could not allocate read-only space for shared library \"%s\".\n",
                            rplBasics->moduleNameBuffer);
         return static_cast<int32_t>(tinyHeapError);
      }
   }

   Loader_LogEntry(2, 0, 0, "sLoadOneShared  LiSetupOneRPL start.");

   LiSetupOneRPL(kernel::UniqueProcessId::Invalid, rplBasics, sData->gpSharedCodeHeapTracking, sData->gpSharedReadHeapTracking);
}

int32_t
LiInitSharedForAll()
{
   virt_ptr<void> outAllocPtr;
   auto tinyHeapError = TinyHeap_Alloc(sData->gpSharedCodeHeapTracking, 0x430, -4, &outAllocPtr);
   if (tinyHeapError < TinyHeapError::OK) {
      return static_cast<int32_t>(tinyHeapError);
   }

   sData->sgpTrackComp = virt_cast<TinyHeap *>(outAllocPtr);
   tinyHeapError = TinyHeap_Setup(sData->sgpTrackComp, 0x430, virt_cast<void *>(DataAreaBase), 0x60000000); // 1536 mb??
   if (tinyHeapError < TinyHeapError::OK) {
      TinyHeap_Free(sData->gpSharedCodeHeapTracking, virt_cast<void *>(sData->sgpTrackComp));
      return static_cast<int32_t>(tinyHeapError);
   }

   sLoadOneShared("coreinit.rpl");
}

int32_t
LiInitSharedForProcess(virt_ptr<RPL_STARTINFO> rplStartInfo)
{
   if (sData->upid != kernel::UniqueProcessId::Root) {
      if (sData->gProcFlags & 2) {
         // Shared libraries disabled in process
         rplStartInfo->unk_0x04 = 0x10000000;
         return 0;
      }
   }

   if ((sData->gProcFlags & 1) == 0) {
      LiInitSharedForAll();
   }

}
}

// LOADER_Init
int32_t
init(kernel::UniqueProcessId upid,
     uint32_t num_codearea_heap_blocks,
     uint32_t max_codesize,
     uint32_t r6, // maybe max_size ?
     virt_ptr<uint32_t> r7,
     virt_ptr<uint32_t> r8,
     virt_ptr<RPL_STARTINFO> rplStartInfo)
{
   sData->upid = upid;
   sData->codeAreaNumBlocks = num_codearea_heap_blocks;

   if (max_codesize > MaxCodeSize) {
      max_codesize = MaxCodeSize;
   }

   sData->maxCodeSize = max_codesize;
   sData->unk_0xEFE01018 = r6;
   rplStartInfo->unk_0x24 = DataAreaBase + r6;

   sData->codeAreaTrackingSize = (num_codearea_heap_blocks * TinyHeapBlockSize) + TinyHeapHeaderSize;
   sData->codeAreaSize = sData->maxCodeSize - align_up(sData->codeAreaTrackingSize, 64);
   sData->codeAreaTracking = virt_cast<TinyHeap *>(DataAreaBase - align_up(sData->codeAreaTrackingSize, 64));

   sData->dataAreaAllocations = 0u;
   sData->dataAreaSize = 0u;

   std::string_view moduleName;
   bool isRoot = false;

   if (upid == kernel::UniqueProcessId::Root) {
      moduleName = "root.rpx";
      isRoot = true;
   } else {
      moduleName = virt_addrof(sData->moduleName).getRawPointer();
      isRoot = false;
   }

   auto tinyHeapError = TinyHeap_Setup(sData->codeAreaTracking,
                                       sData->codeAreaTrackingSize,
                                       virt_cast<void *>(virt_cast<virt_addr>(sData->codeAreaTracking) - sData->codeAreaSize),
                                       sData->codeAreaSize);
   if (tinyHeapError < TinyHeapError::OK) {
      // LiSetFatalError(r27, r19, 1, "LOADER_Init", 0xCC);
      // Loader_ReportError("*** Process code heap setup failed.\n");
      // gUPID = 0
   }

   auto coreId = cpu::this_core::id();
   if (!sData->gIPCLInitialized[coreId]) {
      internal::IPCLDriver_Init();
      internal::IPCLDriver_Open();
      internal::LiInitIopInterface();
      sData->gIPCLInitialized[coreId] = TRUE;
   }

   internal::LiInitSharedForProcess(rplStartInfo);

   // ... stuff ..
   internal::LiInitBuffer(true);

   return 0;
}

// __LoaderStart
void
start(void *entryParams, void *startupParams)
{
   if (entryParams) {
      return entry(entryParams);
   }

   // Start up
   sData->gProcFlags = gIpcData->procFlags;

   auto loaderSharedAddr = static_cast<virt_addr>(0xFA000000);
   sData->gpLoaderShared = virt_cast<LoaderShared *>(loaderSharedAddr);
   sData->gpSharedCodeHeapTracking = virt_cast<TinyHeap *>(loaderSharedAddr + sizeof(LoaderShared));
   sData->gpSharedReadHeapTracking = virt_cast<TinyHeap *>(loaderSharedAddr + sizeof(LoaderShared) + SharedCodeTrackingSize);

   if (sData->gProcFlags & 1) {
      TinyHeap_Setup(sData->gpSharedCodeHeapTracking,
                     SharedCodeTrackingSize,
                     virt_cast<void *>(static_cast<virt_addr>(0x01000000)),
                     0x007E0000);

      TinyHeap_Setup(sData->gpSharedReadHeapTracking,
                     SharedReadTrackingSize,
                     virt_cast<void *>(static_cast<virt_addr>(0xF8000000)),
                     0x03000000);

      // Shared code for loader
      TinyHeap_AllocAt(sData->gpSharedCodeHeapTracking,
                       virt_cast<void *>(static_cast<virt_addr>(0x01000000)),
                       align_up(0x1C758, 1024));

      // Reserve space for heap tracking
      TinyHeap_AllocAt(sData->gpSharedReadHeapTracking,
                       virt_cast<void *>(static_cast<virt_addr>(0xF8000000)),
                       0x02000000);

      TinyHeap_AllocAt(sData->gpSharedReadHeapTracking,
                       virt_cast<void *>(loaderSharedAddr),
                       0x18);

      TinyHeap_AllocAt(sData->gpSharedReadHeapTracking,
                       virt_cast<void *>(loaderSharedAddr + sizeof(LoaderShared)),
                       0x1860);

      // Clear gpLoaderShared
      std::memset(sData->gpLoaderShared.getRawPointer(), 0, sizeof(LoaderShared));
   }

   auto result = init(gIpcData->upid,
                      gIpcData->num_codearea_heap_blocks,
                      gIpcData->max_codesize,
                      gIpcData->unk_0xEFE0A418,
                      virt_addrof(gIpcData->unk_0xEFE0A420),
                      virt_addrof(gIpcData->unk_0xEFE0A424),
                      virt_addrof(gIpcData->rplStartInfo));
   if (result) {

   }
}

} // namespace cafe::loader

#endif
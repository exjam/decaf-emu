#pragma optimize("", off)
#include "cafe_loader_bounce.h"
#include "cafe_loader_entry.h"
#include "cafe_loader_error.h"
#include "cafe_loader_globals.h"
#include "cafe_loader_heap.h"
#include "cafe_loader_iop.h"
#include "cafe_loader_load_setup.h"
#include "cafe_loader_log.h"
#include "cafe_loader_minfileinfo.h"
#include "cafe/cafe_stackobject.h"

#include <common/align.h>
#include <zlib.h>

namespace cafe::loader::internal
{

struct SegmentBounds
{
   uint32_t min = -1;
   uint32_t max = 0;
   uint32_t allocMax = 0;
   const char *name = nullptr;
};

struct RplSegmentBounds
{
   SegmentBounds data;
   SegmentBounds load;
   SegmentBounds text;
   SegmentBounds temp;

   SegmentBounds &operator[](size_t idx)
   {
      if (idx == 0) {
         return data;
      } else if (idx == 1) {
         return load;
      } else if (idx == 2) {
         return text;
      } else {
         return temp;
      }
   }
};

static void
LiClearUserBss(bool userHasControl,
               kernel::UniqueProcessId upid,
               virt_ptr<void> base,
               uint32_t size)
{
   if (!userHasControl) {
      // Check upid
      if (upid == kernel::UniqueProcessId::HomeMenu ||
          upid == kernel::UniqueProcessId::OverlayMenu ||
          upid == kernel::UniqueProcessId::ErrorDisplay ||
          upid == kernel::UniqueProcessId::Game) {
         return;
      }
   }

   std::memset(base.getRawPointer(), 0, size);
}

static int32_t
GetNextBounce(virt_ptr<LOADED_RPL> rpl)
{
   uint32_t chunkBytesRead = 0;
   auto error = LiWaitOneChunk(&chunkBytesRead,
                               virt_cast<char *>(rpl->pathBuffer).getRawPointer(),
                               rpl->fileType);
   if (error != 0) {
      return error;
   }

   rpl->fileOffset = rpl->upcomingFileOffset;
   rpl->upcomingFileOffset += chunkBytesRead;
   rpl->totalBytesRead += chunkBytesRead;
   rpl->lastChunkBuffer = rpl->chunkBuffer;

   auto readBufferNumber = rpl->upcomingBufferNumber;
   if (readBufferNumber == 1) {
      rpl->upcomingBufferNumber = 2u;

      rpl->virtualFileBaseOffset += chunkBytesRead;
   } else {
      rpl->upcomingBufferNumber = 1u;

      rpl->virtualFileBase = virt_cast<void *>(virt_cast<virt_addr>(rpl->virtualFileBase) - rpl->virtualFileBaseOffset);
      rpl->virtualFileBaseOffset = chunkBytesRead;
   }

   if (chunkBytesRead == 0x400000) {
      return LiRefillUpcomingBounceBuffer(rpl, readBufferNumber);
   }

   LiInitBuffer(false);
   return 0;
}

static int32_t
sLiPrepareBounceBufferForReading(virt_ptr<LOADED_RPL> rpl,
                                 uint32_t sectionIndex,
                                 std::string_view name,
                                 uint32_t sectionOffset,
                                 uint32_t *outBytesRead,
                                 uint32_t readSize,
                                 virt_ptr<void> *outBuffer)
{
   LiCheckAndHandleInterrupts();

   if (rpl->upcomingFileOffset <= rpl->fileOffset) {
      if (sectionIndex == 0) {
         Loader_ReportError("*** {} Segment {}: bounce buffer has no size or is corrupted.",
                            rpl->moduleNameBuffer, name);
      } else {
         Loader_ReportError("*** {} Segment {}'s segment {}: bounce buffer has no size or is corrupted.",
                            rpl->moduleNameBuffer, name, sectionIndex);
      }

      LiSetFatalError(0x18729B, rpl->fileType, 1, "sLiPrepareBounceBufferForReading", 0xAD);
      return -470074;
   }

   while (sectionOffset >= rpl->upcomingFileOffset) {
      auto error = GetNextBounce(rpl);
      if (error != 0) {
         break;
      }
   }

   if (sectionOffset < rpl->fileOffset) {
      if (sectionIndex <= 0) {
         Loader_ReportError(
            "*** {} Segment {}'s segment {} offset is before the current read position.",
            rpl->moduleNameBuffer, name, sectionIndex);
      } else {
         Loader_ReportError("*** {} Segment {}'s base is before the current read position.",
                            rpl->moduleNameBuffer, name);
      }

      LiSetFatalError(0x18729Bu, rpl->fileType, 1, "sLiPrepareBounceBufferForReading", 193);
      return -470075;
   }

   if (sectionOffset == rpl->upcomingFileOffset) {
      if (sectionIndex <= 0) {
         Loader_ReportError(
            "*** {} Segment {}'s segment {}: file has nothing left to read or bounce buffer is corrupted.",
            rpl->moduleNameBuffer, name, sectionIndex);
      } else {
         Loader_ReportError(
            "*** {} Segment {}: file has nothing left to read or bounce buffer is corrupted.",
            rpl->moduleNameBuffer, name);
      }
      LiSetFatalError(0x18729Bu, rpl->fileType, 1, "sLiPrepareBounceBufferForReading", 210);
      return -470076;
   }

   auto bytesRead = rpl->upcomingFileOffset - sectionOffset;
   if (readSize < bytesRead) {
      bytesRead = readSize;
   }

   *outBytesRead = bytesRead;
   *outBuffer = virt_cast<void *>(virt_cast<virt_addr>(rpl->virtualFileBase) + sectionOffset);
   return 0;
}

int32_t
sLiRefillBounceBufferForReading(virt_ptr<LOADED_RPL> basics,
                                uint32_t *outBytesRead,
                                uint32_t size,
                                virt_ptr<void> *outBuffer)
{
   LiCheckAndHandleInterrupts();
   *outBytesRead = 0;
   auto result = GetNextBounce(basics);
   if (!result) {
      auto bytesRead = basics->upcomingFileOffset - basics->fileOffset;
      if (size < bytesRead) {
         bytesRead = size;
      }

      *outBytesRead = bytesRead;
      *outBuffer = basics->lastChunkBuffer;
   }

   return result;
}

int32_t
ZLIB_UncompressFromStream(virt_ptr<LOADED_RPL> basics,
                          uint32_t sectionIndex,
                          std::string_view boundsName,
                          uint32_t fileOffset,
                          uint32_t deflatedSize,
                          virt_ptr<void> inflatedBuffer,
                          uint32_t *inflatedSize)
{
   auto inflatedBytesMax = *inflatedSize;
   auto deflatedBytesRemaining = deflatedSize;
   auto bounceBuffer = virt_ptr<void> { nullptr };
   auto bounceBufferSize = uint32_t { 0 };

   LiCheckAndHandleInterrupts();

   auto error =
      sLiPrepareBounceBufferForReading(basics, sectionIndex, boundsName,
                                       fileOffset, &bounceBufferSize,
                                       deflatedSize, &bounceBuffer);
   if (error) {
      return error;
   }

   auto stream = z_stream { };
   std::memset(&stream, 0, sizeof(stream));

   auto zlibError = inflateInit(&stream);
   if (zlibError != Z_OK) {
      switch (zlibError) {
      case Z_STREAM_ERROR:
         LiSetFatalError(0x18729Bu, basics->fileType, 1, "ZLIB_UncompressFromStream", 332);
         return Error::ZlibStreamError;
      case Z_MEM_ERROR:
         LiSetFatalError(0x187298u, basics->fileType, 0, "ZLIB_UncompressFromStream", 319);
         return Error::ZlibMemError;
      case Z_VERSION_ERROR:
         LiSetFatalError(0x18729Bu, basics->fileType, 1, "ZLIB_UncompressFromStream", 332);
         return Error::ZlibVersionError;
      default:
         Loader_ReportError("***Unknown ZLIB error {} (0x{}).", zlibError, zlibError);
         LiSetFatalError(0x18729Bu, basics->fileType, 1, "ZLIB_UncompressFromStream", 332);
         return Error::ZlibUnknownError;
      }
   }

   constexpr auto InflateChunkSize = 0x3000u;
   basics->lastSectionCrc = 0u;

   while (true) {
      // TODO: Loader_UpdateHeartBeat();
      LiCheckAndHandleInterrupts();

      stream.avail_in = bounceBufferSize;
      stream.next_in = reinterpret_cast<Bytef *>(bounceBuffer.getRawPointer());
      stream.next_out = reinterpret_cast<Bytef *>(inflatedBuffer.getRawPointer());

      while (stream.avail_in) {
         LiCheckAndHandleInterrupts();
         stream.avail_out = std::min<uInt>(InflateChunkSize, inflatedBytesMax - stream.total_out);

         zlibError = inflate(&stream, 0);
         if (zlibError != Z_OK && zlibError != Z_STREAM_END) {
            switch (zlibError) {
            case Z_STREAM_ERROR:
               LiSetFatalError(0x18729Bu, basics->fileType, 1, "ZLIB_UncompressFromStream", 405);
               return -470086;
            case Z_MEM_ERROR:
               LiSetFatalError(0x187298u, basics->fileType, 0, "ZLIB_UncompressFromStream", 415);
               error = -470084;
               break;
            case Z_DATA_ERROR:
               LiSetFatalError(0x18729Bu, basics->fileType, 1, "ZLIB_UncompressFromStream", 419);
               error = -470087;
               break;
            default:
               Loader_ReportError("***Unknown ZLIB error {} (0x{}).", zlibError, zlibError);
               LiSetFatalError(0x18729Bu, basics->fileType, 1, "ZLIB_UncompressFromStream", 424);
               error = -470100;
            }

            inflateEnd(&stream);
            return error;
         }

         decaf_check(stream.total_out <= inflatedBytesMax);
      }

      deflatedBytesRemaining -= bounceBufferSize;
      if (!deflatedBytesRemaining) {
         break;
      }

      error = sLiRefillBounceBufferForReading(basics, &bounceBufferSize, deflatedBytesRemaining, &bounceBuffer);
      if (error) {
         LiSetFatalError(0x18729Bu, basics->fileType, 1, "ZLIB_UncompressFromStream", 520);
         inflateEnd(&stream);
         *inflatedSize = stream.total_out;
         return -470087;
      }
   }

   inflateEnd(&stream);
   *inflatedSize = stream.total_out;
   return 0;
}

static int32_t
LiSetupOneAllocSection(kernel::UniqueProcessId upid,
                       virt_ptr<LOADED_RPL> basics,
                       int32_t sectionIndex,
                       virt_ptr<rpl::SectionHeader> sectionHeader,
                       int32_t unk_a5,
                       SegmentBounds *bounds,
                       virt_ptr<void> base,
                       uint32_t baseAlign,
                       uint32_t unk_a9)
{
   auto globals = getGlobalStorage();
   LiCheckAndHandleInterrupts();

   auto sectionAddress = virt_cast<virt_addr>(base) + (sectionHeader->addr - bounds->min);
   basics->sectionAddressBuffer[sectionIndex] = sectionAddress;

   if (!align_check(sectionAddress, sectionHeader->addralign)) {
      Loader_ReportError("***{} section {} alignment failure.", bounds->name, sectionIndex);
      Loader_ReportError("Ptr              = 0x{:08X}", sectionAddress);
      Loader_ReportError("{} base        = 0x{:08X}", bounds->name, base);
      Loader_ReportError("{} base align  = {}", bounds->name, baseAlign);
      Loader_ReportError("SecHdr->addr     = 0x{:08X}", sectionHeader->addr);
      Loader_ReportError("bound[{}].base = 0x{:08X}", bounds->name, bounds->min);
      LiSetFatalError(0x18729Bu, basics->fileType, 1, "sLiSetupOneAllocSection", 1510);
      return -470043;
   }

   if (!unk_a9 && sectionHeader->type == rpl::SHT_NOBITS && unk_a5) {
      auto userHasControl =
         (upid == kernel::UniqueProcessId::Invalid) ? true : globals->userHasControl;
      LiClearUserBss(userHasControl, upid, virt_cast<void *>(sectionAddress), sectionHeader->size);
   } else {
      auto bytesAvailable = uint32_t { 0 };
      auto sectionData = virt_ptr<void> { nullptr };
      auto error = sLiPrepareBounceBufferForReading(basics,
                                                    sectionIndex,
                                                    bounds->name,
                                                    sectionHeader->offset,
                                                    &bytesAvailable,
                                                    sectionHeader->size,
                                                    &sectionData);
      if (error) {
         return error;
      }

      if (!unk_a9 && (sectionHeader->flags & rpl::SHF_DEFLATED)) {
         auto inflatedExpectedSizeBuffer = std::array<uint8_t, sizeof(uint32_t)> { };
         auto readBytes = uint32_t { 0 };

         while (readBytes < inflatedExpectedSizeBuffer.size()) {
            std::memcpy(
               inflatedExpectedSizeBuffer.data() + readBytes,
               sectionData.getRawPointer(),
               std::min<size_t>(bytesAvailable, inflatedExpectedSizeBuffer.size() - readBytes));

            readBytes += bytesAvailable;
            if (readBytes >= inflatedExpectedSizeBuffer.size()) {
               break;
            }

            error = sLiRefillBounceBufferForReading(basics,
                                                    &bytesAvailable,
                                                    inflatedExpectedSizeBuffer.size() - readBytes,
                                                    &sectionData);
            if (error) {
               return error;
            }
         }

         auto inflatedExpectedSize = *reinterpret_cast<be2_val<uint32_t> *>(inflatedExpectedSizeBuffer.data());
         if (inflatedExpectedSize) {
            auto inflatedBytes = static_cast<uint32_t>(inflatedExpectedSize);
            error = ZLIB_UncompressFromStream(basics,
                                              sectionIndex,
                                              bounds->name,
                                              sectionHeader->offset + 4,
                                              sectionHeader->size - 4,
                                              virt_cast<void *>(sectionAddress),
                                              &inflatedBytes);
            if (error) {
               Loader_ReportError(
                  "***{} {} {} Decompression ({}->{}) failure.",
                  basics->moduleNameBuffer,
                  bounds->name,
                  sectionIndex,
                  sectionHeader->size - 4,
                  inflatedExpectedSize);
               return error;
            }

            if (inflatedBytes != inflatedExpectedSize) {
               Loader_ReportError(
                  "***{} {} {} Decompression ({}->{}) failure. Anticipated uncompressed size would be {}; got {}",
                  basics->moduleNameBuffer,
                  bounds->name,
                  sectionIndex,
                  sectionHeader->size - 4,
                  inflatedExpectedSize,
                  inflatedExpectedSize,
                  inflatedBytes);
               LiSetFatalError(0x18729Bu, basics->fileType, 1, "sLiSetupOneAllocSection", 1604);
               return -470090;
            }

            sectionHeader->size = inflatedBytes;
         }
      } else {
         auto bytesRead = 0u;
         while (true) {
            // TODO: Loader_UpdateHeartBeat();
            LiCheckAndHandleInterrupts();
            std::memcpy(virt_cast<void *>(sectionAddress + bytesRead).getRawPointer(),
                        sectionData.getRawPointer(),
                        bytesAvailable);
            bytesRead += bytesAvailable;

            if (bytesRead >= sectionHeader->size) {
               break;
            }

            error = sLiRefillBounceBufferForReading(basics,
                                                    &bytesAvailable,
                                                    sectionHeader->size - bytesRead,
                                                    &sectionData);
            if (error) {
               return error;
            }
         }
      }
   }

   bounds->allocMax = std::max<uint32_t>(bounds->allocMax,
                                         sectionHeader->addr + sectionHeader->size);
   if (bounds->allocMax > bounds->max) {
      Loader_ReportError(
         "*** %s section %d segment %s makerpl's segment size was wrong: real time calculated size =0x%08X makerpl's size=0x%08X.",
         basics->moduleNameBuffer,
         sectionIndex,
         bounds->name,
         bounds->allocMax,
         bounds->max);
      LiSetFatalError(0x18729Bu, basics->fileType, 1, "sLiSetupOneAllocSection", 1676);
      return -470091;
   }

   return 0;
}

int32_t
LiSetupOneRPL(kernel::UniqueProcessId upid,
              virt_ptr<LOADED_RPL> basics,
              virt_ptr<TinyHeap> codeHeapTracking,
              virt_ptr<TinyHeap> dataHeapTracking)
{
   int32_t result = 0;

   // Calculate segment bounds
   RplSegmentBounds bounds;
   bounds.data.name = "DATA";
   bounds.load.name = "LOADERINFO";
   bounds.text.name = "TEXT";
   bounds.temp.name = "TEMP";

   auto shBase = virt_cast<virt_addr>(basics->sectionHeaderBuffer);
   for (auto i = 1u; i < basics->elfHeader.shnum; ++i) {
      auto sectionHeader = virt_cast<rpl::SectionHeader *>(shBase + i * basics->elfHeader.shentsize);
      if (sectionHeader->size == 0 ||
          sectionHeader->type == rpl::SHT_RPL_FILEINFO ||
          sectionHeader->type == rpl::SHT_RPL_CRCS ||
          sectionHeader->type == rpl::SHT_RPL_IMPORTS) {
         continue;
      }

      if (sectionHeader->flags & rpl::SHF_ALLOC) {
         if ((sectionHeader->flags & rpl::SHF_EXECINSTR) &&
             sectionHeader->type != rpl::SHT_RPL_EXPORTS) {
            bounds.text.min = std::min<uint32_t>(bounds.text.min, sectionHeader->addr);
         } else {
            if (sectionHeader->flags & rpl::SHF_WRITE) {
               bounds.data.min = std::min<uint32_t>(bounds.data.min, sectionHeader->addr);
            } else {
               bounds.load.min = std::min<uint32_t>(bounds.load.min, sectionHeader->addr);
            }
         }
      } else {
         bounds.temp.min = std::min<uint32_t>(bounds.temp.min, sectionHeader->offset);
         bounds.temp.max = std::max<uint32_t>(bounds.temp.max, sectionHeader->offset + sectionHeader->size);
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

   auto fileInfo = basics->fileInfoBuffer;
   bounds.text.max = (bounds.text.min + fileInfo->textSize) - fileInfo->trampAdjust;
   bounds.data.max = bounds.data.min + fileInfo->dataSize;
   bounds.load.max = (bounds.load.min + fileInfo->loadSize) - fileInfo->fileInfoPad;

   auto textSize = static_cast<uint32_t>(bounds.text.max - bounds.text.min);
   auto dataSize = static_cast<uint32_t>(bounds.data.max - bounds.data.min);
   auto loadSize = static_cast<uint32_t>(bounds.load.max - bounds.load.min);
   auto tempSize = static_cast<uint32_t>(bounds.temp.max - bounds.temp.min);

   if (fileInfo->trampAdjust >= textSize ||
       fileInfo->textSize - fileInfo->trampAdjust < textSize ||
       fileInfo->dataSize < dataSize ||
       fileInfo->loadSize - fileInfo->fileInfoPad < loadSize ||
       fileInfo->tempSize < tempSize) {
      Loader_ReportError("***Bounds check failure.");
      Loader_ReportError("b%d: %08X %08x", 0, bounds.data.min, bounds.data.max);
      Loader_ReportError("b%d: %08X %08x", 1, bounds.load.min, bounds.load.max);
      Loader_ReportError("b%d: %08X %08x", 2, bounds.text.min, bounds.text.max);
      Loader_ReportError("b%d: %08X %08x", 3, bounds.temp.min, bounds.temp.max);
      Loader_ReportError("TrampAdj = %08X", fileInfo->trampAdjust);
      Loader_ReportError("Text = %08X", fileInfo->textSize);
      Loader_ReportError("Data = %08X", fileInfo->dataSize);
      Loader_ReportError("Read = %08X", fileInfo->loadSize - fileInfo->fileInfoPad);
      Loader_ReportError("Temp = %08X", fileInfo->tempSize);
      LiSetFatalError(0x18729B, basics->fileType, 1, "LiSetupOneRPL", 0x715);
      result = -470042;
      goto error;
   }

   if (basics->dataBuffer) {
      for (auto i = 1u; i < basics->elfHeader.shnum; ++i) {
         auto sectionHeader = virt_cast<rpl::SectionHeader *>(shBase + i * basics->elfHeader.shentsize);
         LiCheckAndHandleInterrupts();

         if (sectionHeader->size &&
             !basics->sectionAddressBuffer[i] &&
             (sectionHeader->flags & rpl::SHF_ALLOC) &&
             (sectionHeader->flags & rpl::SHF_WRITE)) {
            result = LiSetupOneAllocSection(upid,
                                            basics,
                                            i,
                                            sectionHeader,
                                            1,
                                            &bounds.data,
                                            basics->dataBuffer,
                                            fileInfo->dataAlign,
                                            0);
            if (result) {
               goto error;
            }
         }
      }
   }

   if (basics->loadBuffer) {
      for (auto i = 1u; i < basics->elfHeader.shnum; ++i) {
         auto sectionHeader = virt_cast<rpl::SectionHeader *>(shBase + i * basics->elfHeader.shentsize);
         LiCheckAndHandleInterrupts();

         if (sectionHeader->size &&
             !basics->sectionAddressBuffer[i] &&
             (sectionHeader->flags & rpl::SHF_ALLOC)) {
            if (sectionHeader->type == rpl::SHT_RPL_EXPORTS ||
                sectionHeader->type == rpl::SHT_RPL_IMPORTS ||
                !(sectionHeader->flags & (rpl::SHF_EXECINSTR | rpl::SHF_WRITE))) {
               result = LiSetupOneAllocSection(upid,
                                               basics,
                                               i,
                                               sectionHeader,
                                               0,
                                               &bounds.load,
                                               basics->loadBuffer,
                                               fileInfo->loadAlign,
                                               (sectionHeader->type == rpl::SHT_RPL_IMPORTS) ? 1 : 0);
               if (result) {
                  goto error;
               }

               if (sectionHeader->type == rpl::SHT_RPL_EXPORTS) {
                  if (sectionHeader->flags & rpl::SHF_EXECINSTR) {
                     basics->numFuncExports = *virt_cast<uint32_t *>(basics->sectionAddressBuffer[i]);
                     basics->funcExports = virt_cast<void *>(basics->sectionAddressBuffer[i] + 8);
                  } else {
                     basics->numDataExports = *virt_cast<uint32_t *>(basics->sectionAddressBuffer[i]);
                     basics->dataExports = virt_cast<void *>(basics->sectionAddressBuffer[i] + 8);
                  }
               }
            }
         }
      }
   }

   if (fileInfo->textSize) {
      if (!basics->textBuffer) {
         Loader_ReportError("Missing TEXT allocation.");
         LiSetFatalError(0x18729Bu, basics->fileType, 1, "LiSetupOneRPL", 1918);
         result = -470057;
         goto error;
      }

      for (auto i = 1u; i < basics->elfHeader.shnum; ++i) {
         auto sectionHeader = virt_cast<rpl::SectionHeader *>(shBase + i * basics->elfHeader.shentsize);
         LiCheckAndHandleInterrupts();

         if (sectionHeader->size &&
             !basics->sectionAddressBuffer[i] &&
             (sectionHeader->flags & rpl::SHF_ALLOC) &&
             (sectionHeader->flags & rpl::SHF_EXECINSTR) &&
             sectionHeader->type != rpl::SHT_RPL_EXPORTS) {
            result = LiSetupOneAllocSection(upid,
                                            basics,
                                            i,
                                            sectionHeader,
                                            0,
                                            &bounds.text,
                                            virt_cast<void *>(virt_cast<virt_addr>(basics->textBuffer) + fileInfo->trampAdjust),
                                            fileInfo->textAlign,
                                            0);
            if (result) {
               goto error;
            }
         }
      }
   }

   if (bounds.temp.min != bounds.temp.max) {
      auto compressedRelocationsBuffer = virt_ptr<void> { nullptr };
      auto compressedRelocationsBufferSize = uint32_t { 0 };
      auto memoryAvailable = uint32_t { 0 };
      auto tempSize = bounds.temp.max - bounds.temp.min;
      auto dataSize = uint32_t { 0 };
      auto data = virt_ptr<void> { nullptr };
      auto readBytes = 0u;

      result = LiCacheLineCorrectAllocEx(codeHeapTracking,
                                         tempSize,
                                         -32,
                                         &compressedRelocationsBuffer,
                                         1,
                                         &compressedRelocationsBufferSize,
                                         &memoryAvailable,
                                         basics->fileType);
      if (result) {
         Loader_ReportError(
            "*** allocation failed for {} size = {}, align = {} from {} heap;  (needed {}, available {}).",
            "compressed relocations",
            tempSize,
            -32,
            "RPL Code",
            compressedRelocationsBufferSize,
            memoryAvailable);
         goto error;
      }

      basics->compressedRelocationsBuffer = compressedRelocationsBuffer;
      basics->compressedRelocationsBufferSize = compressedRelocationsBufferSize;

      result = sLiPrepareBounceBufferForReading(basics, 0, bounds.temp.name, bounds.temp.min, &dataSize, tempSize, &data);
      if (result) {
         goto error;
      }

      while (tempSize > 0) {
         // TODO: Loader_UpdateHeartBeat
         LiCheckAndHandleInterrupts();
         std::memcpy(virt_cast<void *>(virt_cast<virt_addr>(compressedRelocationsBuffer) + readBytes).getRawPointer(),
                     data.getRawPointer(),
                     dataSize);
         readBytes += dataSize;
         tempSize -= dataSize;

         if (!tempSize) {
            break;
         }

         result = sLiRefillBounceBufferForReading(basics, &dataSize, tempSize, &data);
         if (result) {
            goto error;
         }
      }

      for (auto i = 1u; i < basics->elfHeader.shnum; ++i) {
         auto sectionHeader = virt_cast<rpl::SectionHeader *>(shBase + i * basics->elfHeader.shentsize);
         LiCheckAndHandleInterrupts();

         if (sectionHeader->size && !basics->sectionAddressBuffer[i]) {
            basics->sectionAddressBuffer[i] = virt_cast<virt_addr>(compressedRelocationsBuffer) + (sectionHeader->offset - bounds.temp.min);
            bounds.temp.allocMax = std::max<uint32_t>(bounds.temp.allocMax,
                                                      sectionHeader->addr + sectionHeader->size);

            if (bounds.temp.allocMax > bounds.temp.max) {
               Loader_ReportError(
                  "***Section {} segment {} makerpl's section size was wrong: mRealTimeLimit=0x%08X, mLimit=0x%08X. Error is Loader's fault.",
                  i,
                  bounds.temp.name,
                  bounds.temp.allocMax,
                  bounds.temp.max);
               LiSetFatalError(0x18729Bu, basics->fileType, 1, "LiSetupOneRPL", 2034);
               result = -470091;
               goto error;
            }
         }
      }
   }

   for (auto i = 0u; i < 4; ++i) {
      if (bounds[i].allocMax > bounds[i].max) {
         Loader_ReportError(
            "***Segment %s makerpl's segment size was wrong: mRealTimeLimit=0x%08X, mLimit=0x%08X. Error is Loader's fault.\n",
            bounds[i].name,
            bounds[i].allocMax,
            bounds[i].max);
         LiSetFatalError(0x18729Bu, basics->fileType, 1, "LiSetupOneRPL", 2052);
         result = -470091;
         goto error;
      }
   }

   basics->postTrampBuffer = align_up(virt_cast<virt_addr>(basics->textBuffer) + fileInfo->trampAdjust + (bounds.text.allocMax - bounds.text.min), 16);

   basics->unk0xD0 = virt_cast<virt_addr>(basics->textBuffer) + fileInfo->trampAdjust;
   basics->unk0xD4 = basics->unk0xD0 - bounds.text.min;
   basics->unk0xD8 = static_cast<uint32_t>(bounds.text.max - bounds.text.min);

   basics->unk0xDC = virt_cast<virt_addr>(basics->dataBuffer);
   basics->unk0xE0 = basics->unk0xDC - bounds.data.min;
   basics->unk0xE4 = static_cast<uint32_t>(bounds.data.max - bounds.data.min);

   basics->unk0xE8 = virt_cast<virt_addr>(basics->loadBuffer);
   basics->unk0xEC = basics->unk0xE8 - bounds.load.min;
   basics->unk0xF0 = static_cast<uint32_t>(bounds.load.max - bounds.load.min);

   basics->loadStateFlags |= LoadStateFlags::LoaderSetup;

   result = LiCleanUpBufferAfterModuleLoaded();
   if (result) {
      goto error;
   }

   return 0;

error:
   if (basics->compressedRelocationsBuffer) {
      LiCacheLineCorrectFreeEx(codeHeapTracking,
                                 basics->compressedRelocationsBuffer,
                                 basics->compressedRelocationsBufferSize);
      basics->compressedRelocationsBuffer = nullptr;
   }

   LiCloseBufferIfError();
   Loader_ReportError("***LiSetupOneRPL({}) failed with err={}.", basics->moduleNameBuffer, result);
   return result;
}

int32_t
LOADER_Setup(kernel::UniqueProcessId upid,
             virt_ptr<void> kernelHandle,
             BOOL isPurge,
             virt_ptr<LOADER_MinFileInfo> minFileInfo)
{
   auto globals = getGlobalStorage();
   if (globals->currentUpid != upid) {
      Loader_ReportError("*** Loader address space not set for process {} but called for {}.",
                         static_cast<int>(globals->currentUpid.value()),
                         static_cast<int>(upid));
      LiSetFatalError(0x18729Bu, 0, 1, "LOADER_Setup", 2184);
      LiCloseBufferIfError();
      return -470008;
   }

   if (minFileInfo) {
      if (auto error = LiValidateMinFileInfo(minFileInfo, "LOADER_Setup")) {
         LiCloseBufferIfError();
         return error;
      }
   }

   auto error = LiValidateAddress(kernelHandle, 0, 0, -470046,
                                  virt_addr { 0x2000000 },
                                  virt_addr { 0x10000000 },
                                  0);
   if (error) {
      if (getProcFlags().disableSharedLibraries()) {
         Loader_ReportError("*** bad kernel handle for module (out of valid range-read)");
         LiSetFatalError(0x18729Bu, 0, 1, "LOADER_Setup", 2215);
         LiCloseBufferIfError();
         return error;
      }

      error = LiValidateAddress(kernelHandle, 0, 0, -470046,
                                virt_addr { 0xEFE0B000 },
                                virt_addr { 0xEFE80000 },
                                "kernel handle for module (out of valid range-read)");
      if (error) {
         LiSetFatalError(0x18729Bu, 0, 1, "LOADER_Setup", 2215);
         LiCloseBufferIfError();
         return error;
      }
   }

   // TODO: Finish LOADER_Setup
   return -1;
}

} // namespace cafe::loader::internal

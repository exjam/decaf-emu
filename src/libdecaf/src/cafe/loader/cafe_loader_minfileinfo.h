#pragma once
#include "cafe_loader_load_basics.h"
#include <libcpu/be2_struct.h>
#include <string_view>

namespace cafe::loader
{

struct LOADER_MinFileInfo
{
   be2_val<uint32_t> size;
   be2_val<uint32_t> version;
   be2_virt_ptr<char> moduleNameBuffer;
   be2_val<uint32_t> moduleNameBufferLen;
   be2_virt_ptr<uint32_t> outKernelHandle;
   be2_virt_ptr<uint32_t> outNumberOfSections;
   be2_virt_ptr<uint32_t> outSectionInfo;
   be2_virt_ptr<uint32_t> outSizeOfFileInfo;
   be2_virt_ptr<uint32_t> outFileInfo;
   be2_val<uint32_t> dataSize;
   be2_val<uint32_t> dataAlign;
   be2_virt_ptr<uint32_t> dataBuffer;
   be2_val<uint32_t> loadSize;
   be2_val<uint32_t> loadAlign;
   be2_virt_ptr<uint32_t> loadBuffer;
   be2_val<uint32_t> fileInfoFlags;
   be2_virt_ptr<uint32_t> inoutNextTlsModuleNumber;
   UNKNOWN(0x4);
   be2_virt_ptr<uint32_t> outPathStringSize;
   be2_virt_ptr<uint32_t> outPathString;
   be2_val<uint32_t> fileLocation;
   UNKNOWN(0xA4 - 0x54);
};
CHECK_OFFSET(LOADER_MinFileInfo, 0x00, size);
CHECK_OFFSET(LOADER_MinFileInfo, 0x04, version);
CHECK_OFFSET(LOADER_MinFileInfo, 0x08, moduleNameBuffer);
CHECK_OFFSET(LOADER_MinFileInfo, 0x0C, moduleNameBufferLen);
CHECK_OFFSET(LOADER_MinFileInfo, 0x10, outKernelHandle);
CHECK_OFFSET(LOADER_MinFileInfo, 0x14, outNumberOfSections);
CHECK_OFFSET(LOADER_MinFileInfo, 0x18, outSectionInfo);
CHECK_OFFSET(LOADER_MinFileInfo, 0x1C, outSizeOfFileInfo);
CHECK_OFFSET(LOADER_MinFileInfo, 0x20, outFileInfo);
CHECK_OFFSET(LOADER_MinFileInfo, 0x24, dataSize);
CHECK_OFFSET(LOADER_MinFileInfo, 0x28, dataAlign);
CHECK_OFFSET(LOADER_MinFileInfo, 0x2C, dataBuffer);
CHECK_OFFSET(LOADER_MinFileInfo, 0x30, loadSize);
CHECK_OFFSET(LOADER_MinFileInfo, 0x34, loadAlign);
CHECK_OFFSET(LOADER_MinFileInfo, 0x38, loadBuffer);
CHECK_OFFSET(LOADER_MinFileInfo, 0x3C, fileInfoFlags);
CHECK_OFFSET(LOADER_MinFileInfo, 0x40, inoutNextTlsModuleNumber);
CHECK_OFFSET(LOADER_MinFileInfo, 0x48, outPathStringSize);
CHECK_OFFSET(LOADER_MinFileInfo, 0x4C, outPathString);
CHECK_OFFSET(LOADER_MinFileInfo, 0x50, fileLocation);
CHECK_SIZE(LOADER_MinFileInfo, 0xA4);

namespace internal
{

int32_t
LiValidateAddress(virt_ptr<void> ptr,
                  uint32_t size,
                  uint32_t alignMask,
                  int32_t errorCode,
                  virt_addr minAddr,
                  virt_addr maxAddr,
                  std::string_view name);

int32_t
LiGetMinFileInfo(virt_ptr<LOADED_RPL> rpl,
                 virt_ptr<LOADER_MinFileInfo> info);

int32_t
LiValidateMinFileInfo(virt_ptr<LOADER_MinFileInfo> minFileInfo,
                      std::string_view funcName);

} // namespace internal

} // namespace cafe::loader

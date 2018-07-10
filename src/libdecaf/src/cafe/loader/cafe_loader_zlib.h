#pragma once
#include "cafe_loader_load_basics.h"

#include <cstdint>
#include <libcpu/be2_struct.h>
#include <string_view>

namespace cafe::loader::internal
{

uint32_t
LiCalcCRC32(uint32_t crc,
            const virt_ptr<void> data,
            uint32_t size);

int32_t
ZLIB_UncompressFromStream(virt_ptr<LOADED_RPL> basics,
                          uint32_t sectionIndex,
                          std::string_view boundsName,
                          uint32_t fileOffset,
                          uint32_t deflatedSize,
                          virt_ptr<void> dst,
                          uint32_t *inflatedSize);

} // namespace cafe::loader::internal

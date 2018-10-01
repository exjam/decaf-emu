#ifndef H264_ENUM_H
#define H264_ENUM_H

#include <common/enum_start.h>

ENUM_NAMESPACE_BEG(cafe)
ENUM_NAMESPACE_BEG(h264)

ENUM_BEG(H264Error, int32_t)
   ENUM_VALUE(OK,                   0)
   ENUM_VALUE(InvalidSps,           26)
   ENUM_VALUE(GenericError,         0x1000000)
   ENUM_VALUE(InvalidParameter,     0x1010000)
   ENUM_VALUE(OutOfMemory,          0x1020000)
   ENUM_VALUE(InvalidProfile,       0x1080000)
ENUM_END(H264Error)

ENUM_NAMESPACE_END(h264)
ENUM_NAMESPACE_END(cafe)

#include <common/enum_end.h>

#endif // ifdef SWKBD_ENUM_H

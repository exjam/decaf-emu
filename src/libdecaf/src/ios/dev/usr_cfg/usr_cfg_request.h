#pragma once
#include "usr_cfg_enum.h"
#include "usr_cfg_types.h"

#include <cstdint>
#include <libcpu/be2_struct.h>
#include <common/structsize.h>

namespace ios
{

namespace dev
{

namespace usr_cfg
{

/**
 * \ingroup ios_dev_usr_cfg
 * @{
 */

#pragma pack(push, 1)

struct UCReadSysConfigRequest
{
   be2_val<uint32_t> count;
   be2_val<uint32_t> size;
   be2_array<UCSysConfig, 1> settings;
};
CHECK_OFFSET(UCReadSysConfigRequest, 0x0, count);
CHECK_OFFSET(UCReadSysConfigRequest, 0x4, size);
CHECK_OFFSET(UCReadSysConfigRequest, 0x8, settings);
CHECK_SIZE(UCReadSysConfigRequest, 0x5C);

struct UCWriteSysConfigRequest
{
   be2_val<uint32_t> count;
   be2_val<uint32_t> size;
   be2_array<UCSysConfig, 1> settings;
};
CHECK_OFFSET(UCWriteSysConfigRequest, 0x0, count);
CHECK_OFFSET(UCWriteSysConfigRequest, 0x4, size);
CHECK_OFFSET(UCWriteSysConfigRequest, 0x8, settings);
CHECK_SIZE(UCWriteSysConfigRequest, 0x5C);

struct UCRequest
{
   union
   {
      be2_struct<UCReadSysConfigRequest> readSysConfigRequest;
      be2_struct<UCWriteSysConfigRequest> writeSysConfigRequest;
   };
};


#pragma pack(pop)

/** @} */

} // namespace usr_cfg

} // namespace dev

} // namespace ios

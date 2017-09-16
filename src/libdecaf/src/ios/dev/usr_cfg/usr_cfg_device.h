#pragma once
#include "ios/ios_device.h"
#include "usr_cfg_enum.h"
#include "usr_cfg_request.h"
#include "usr_cfg_types.h"

#include <cstdint>

namespace ios
{

namespace dev
{

namespace usr_cfg
{

/**
 * \defgroup ios_dev_usr_cfg /dev/usr_cfg
 * \ingroup ios_dev
 * @{
 */

class UserConfigDevice : public IOSDevice
{
public:
   static constexpr const char *Name = "/dev/usr_cfg";

public:
   virtual IOSError
   open(IOSOpenMode mode) override;

   virtual IOSError
   close() override;

   virtual IOSError
   read(void *buffer,
        size_t length) override;

   virtual IOSError
   write(void *buffer,
         size_t length) override;

   virtual IOSError
   ioctl(uint32_t request,
         void *inBuf,
         size_t inLen,
         void *outBuf,
         size_t outLen) override;

   virtual IOSError
   ioctlv(uint32_t request,
          size_t vecIn,
          size_t vecOut,
          IOSVec *vec) override;

private:
   UCError
   readSysConfig(phys_ptr<UCReadSysConfigRequest> request);

   UCError
   writeSysConfig(phys_ptr<UCWriteSysConfigRequest> request);
};

/** @} */

} // namespace usr_cfg

} // namespace dev

} // namespace ios

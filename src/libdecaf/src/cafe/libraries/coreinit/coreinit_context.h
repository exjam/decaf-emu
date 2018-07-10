#pragma once
#include "cafe/kernel/cafe_kernel_context.h"

namespace cafe::coreinit
{

using OSContext = cafe::kernel::Context;
CHECK_SIZE(OSContext, 0x320);

} // cafe::coreinit

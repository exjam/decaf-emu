#pragma once
#include "coreinit_enum.h"

namespace cafe::coreinit
{

void
COSError(COSReportModule module,
         const char *fmt,
         ...);

void
COSWarn(COSReportModule module,
        const char *fmt,
        ...);

void
COSInfo(COSReportModule module,
        const char *fmt,
        ...);

void
COSVerbose(COSReportModule module,
           const char *fmt,
           ...);

} // namespace cafe::coreinit

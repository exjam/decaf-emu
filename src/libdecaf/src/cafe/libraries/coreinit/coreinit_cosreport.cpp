#include "coreinit.h"
#include "coreinit_cosreport.h"
#include "cafe/cafe_stackobject.h"
#include <cstdarg>
#include <cstdio>

namespace cafe::coreinit
{

void
COSVReport(COSReportModule module,
           COSReportLevel level,
           const char *fmt,
           std::va_list va)
{
   StackArray<char, 128> buffer;
   std::vsnprintf(buffer.getRawPointer(), 128, fmt, va);

   // TODO: COSVReport, this ends up in OSConsoleWrite
}


void
COSError(COSReportModule module,
         const char *fmt,
         ...)
{
   va_list va;
   va_start(va, fmt);
   COSVReport(module, COSReportLevel::Error, fmt, va);
}

void
COSWarn(COSReportModule module,
        const char *fmt,
        ...)
{
   va_list va;
   va_start(va, fmt);
   COSVReport(module, COSReportLevel::Warn, fmt, va);
}

void
COSInfo(COSReportModule module,
        const char *fmt,
        ...)
{
   va_list va;
   va_start(va, fmt);
   COSVReport(module, COSReportLevel::Info, fmt, va);
}

void
COSVerbose(COSReportModule module,
           const char *fmt,
           ...)
{
   va_list va;
   va_start(va, fmt);
   COSVReport(module, COSReportLevel::Verbose, fmt, va);
}

} // namespace cafe::coreinit

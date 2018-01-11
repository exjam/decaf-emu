#include "coreinit.h"
#include "coreinit_time.h"
#include "coreinit_systeminfo.h"
#include "decaf_config.h"

#include <common/platform_time.h>
#include <libcpu/cpu.h>

namespace cafe::coreinit
{

static std::chrono::time_point<std::chrono::system_clock>
sEpochTime;

static std::chrono::time_point<std::chrono::system_clock>
sBaseClock;

static cpu::TimerDuration
sBaseTicks;


static uint64_t
scaledTimebase()
{
   if (decaf::config::system::time_scale == 1.0) {
      return cpu::this_core::state()->tb();
   } else {
      // This might introduce timeBase inaccuracies, which is why we only
      //  do it when it is not set to 1.0.
      auto tbDbl = static_cast<double>(cpu::this_core::state()->tb());
      return static_cast<uint64_t>(tbDbl * decaf::config::system::time_scale);
   }
}

/**
 * Time since epoch
 */
OSTime
OSGetTime()
{
   return OSGetSystemTime() + sBaseTicks.count();
}


/**
 * Time since system start up
 */
OSTime
OSGetSystemTime()
{
   return scaledTimebase();
}


/**
 * Ticks since epoch
 */
OSTick
OSGetTick()
{
   return OSGetSystemTick() + static_cast<OSTick>(sBaseTicks.count());
}


/**
 * Ticks since system start up
 */
OSTick
OSGetSystemTick()
{
   return static_cast<uint32_t>(scaledTimebase());
}


/**
 * Convert OSTime to OSCalendarTime
 */
void
OSTicksToCalendarTime(OSTime time,
                      virt_ptr<OSCalendarTime> calendarTime)
{
   auto chrono = coreinit::internal::toTimepoint(time);
   std::time_t system_time_t = std::chrono::system_clock::to_time_t(chrono);
   std::tm tm = platform::localtime(system_time_t);

   calendarTime->tm_sec = tm.tm_sec;
   calendarTime->tm_min = tm.tm_min;
   calendarTime->tm_hour = tm.tm_hour;
   calendarTime->tm_mday = tm.tm_mday;
   calendarTime->tm_mon = tm.tm_mon;
   calendarTime->tm_year = tm.tm_year + 1900; // posix tm_year is year - 1900
   calendarTime->tm_wday = tm.tm_wday;
   calendarTime->tm_yday = tm.tm_yday;
   calendarTime->tm_msec = 0;
   calendarTime->tm_usec = 0;

   // TODO: OSTicksToCalendarTime tm_usec, tm_msec
}


/**
 * Convert OSCalendarTime to OSTime
 */
OSTime
OSCalendarTimeToTicks(virt_ptr<OSCalendarTime> calendarTime)
{
   std::tm tm = { 0 };

   tm.tm_sec = calendarTime->tm_sec;
   tm.tm_min = calendarTime->tm_min;
   tm.tm_hour = calendarTime->tm_hour;
   tm.tm_mday = calendarTime->tm_mday;
   tm.tm_mon = calendarTime->tm_mon;
   tm.tm_year = calendarTime->tm_year - 1900;
   tm.tm_wday = calendarTime->tm_wday;
   tm.tm_yday = calendarTime->tm_yday;
   tm.tm_isdst = -1;

   auto system_time = platform::make_gm_time(tm);
   auto chrono = std::chrono::system_clock::from_time_t(system_time);

   // Add on tm_usec, tm_msec which is missing from std::tm
   chrono += std::chrono::microseconds { calendarTime->tm_usec };
   chrono += std::chrono::milliseconds { calendarTime->tm_msec };

   return coreinit::internal::toOSTime(chrono);
}

void
Library::initialiseClock()
{
   // Calculate the Wii U epoch (01/01/2000)
   std::tm tm = { 0 };
   tm.tm_sec = 0;
   tm.tm_min = 0;
   tm.tm_hour = 0;
   tm.tm_mday = 1;
   tm.tm_mon = 1;
   tm.tm_year = 2000 - 1900;
   tm.tm_isdst = -1;
   sEpochTime = std::chrono::system_clock::from_time_t(platform::make_gm_time(tm));

   sBaseClock = std::chrono::system_clock::now();
   auto ticksSinceEpoch = std::chrono::duration_cast<cpu::TimerDuration>(sBaseClock - sEpochTime);
   auto ticksSinceStart = cpu::TimerDuration { cpu::this_core::state()->tb() };
   sBaseTicks = ticksSinceEpoch - ticksSinceStart;
}

void
Library::registerTimeExports()
{
   RegisterFunctionExport(OSGetTime);
   RegisterFunctionExport(OSGetTick);
   RegisterFunctionExport(OSGetSystemTime);
   RegisterFunctionExport(OSGetSystemTick);
   RegisterFunctionExport(OSTicksToCalendarTime);
   RegisterFunctionExport(OSCalendarTimeToTicks);
}

namespace internal
{

OSTime
msToTicks(OSTime milliseconds)
{
   auto timerSpeed = static_cast<uint64_t>(OSGetSystemInfo()->busSpeed / 4);
   return static_cast<uint64_t>(milliseconds) * (timerSpeed / 1000);
}

OSTime
usToTicks(OSTime microseconds)
{
   auto timerSpeed = static_cast<uint64_t>(OSGetSystemInfo()->busSpeed / 4);
   return static_cast<uint64_t>(microseconds) * (timerSpeed / 1000000);
}

OSTime
nsToTicks(OSTime nanoseconds)
{
   // Division is done in two parts to try to maintain accuracy, 31250 * 32000 = 1*10^9
   auto timerSpeed = static_cast<uint64_t>(OSGetSystemInfo()->busSpeed / 4);
   return (static_cast<uint64_t>(nanoseconds) * (timerSpeed / 31250)) / 32000;
}

OSTime
getBaseTime()
{
   return sBaseTicks.count();
}

std::chrono::time_point<std::chrono::system_clock>
toTimepoint(OSTime time)
{
   auto ticksSinceBaseTime = cpu::TimerDuration { time } - sBaseTicks;
   auto clocksSinceBaseTime = std::chrono::duration_cast<std::chrono::system_clock::duration>(ticksSinceBaseTime);
   return sBaseClock + clocksSinceBaseTime;
}

OSTime
toOSTime(std::chrono::time_point<std::chrono::system_clock> chrono)
{
   auto clocksSinceBaseTime = chrono - sBaseClock;
   auto ticksSinceBaseTime = std::chrono::duration_cast<cpu::TimerDuration>(clocksSinceBaseTime);
   return (sBaseTicks + ticksSinceBaseTime).count();
}

} // namespace internal

} // namespace cafe::coreinit

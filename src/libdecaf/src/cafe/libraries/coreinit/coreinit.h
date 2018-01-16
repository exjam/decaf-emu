#pragma once
#include "cafe/libraries/cafe_hle_library.h"
#include <libcpu/be2_struct.h>

namespace cafe::coreinit
{

struct StaticAlarmData;
struct StaticAllocatorData;
struct StaticAppIoData;
struct StaticClipboardData;
struct StaticDefaultHeapData;
struct StaticEventData;
struct StaticMemHeapData;
struct StaticSchedulerData;
struct StaticSystemInfoData;
struct StaticThreadData;

struct LibraryStaticData
{
   virt_ptr<StaticAlarmData> alarmData;
   virt_ptr<StaticAllocatorData> allocatorData;
   virt_ptr<StaticAppIoData> appIoData;
   virt_ptr<StaticClipboardData> clipboardData;
   virt_ptr<StaticDefaultHeapData> defaultHeapData;
   virt_ptr<StaticEventData> eventData;
   virt_ptr<StaticMemHeapData> memHeapData;
   virt_ptr<StaticSchedulerData> schedulerData;
   virt_ptr<StaticSystemInfoData> systemInfoData;
   virt_ptr<StaticThreadData> threadData;
};

class Library : public cafe::hle::Library
{
public:
   Library() :
      hle::Library(hle::LibraryId::coreinit)
   {
   }

   void libraryEntryPoint() override;

   static virt_ptr<LibraryStaticData> getStaticData();

protected:
   void registerExports() override;
   void registerAlarmExports();
   void registerAllocatorExports();
   void registerAtomicExports();
   void registerAtomic64Exports();
   void registerAppIoExports();
   void registerClipboardExports();
   void registerCoreFunctions();
   void registerCoroutineFunctions();
   void registerEventFunctions();
   void registerInterruptExports();
   void registerMemoryFunctions();
   void registerMemDefaultHeapFunctions();
   void registerMemBlockHeapFunctions();
   void registerMemExpHeapFunctions();
   void registerMemFrameHeapFunctions();
   void registerMemHeapFunctions();
   void registerMemListFunctions();
   void registerMemUnitHeapFunctions();
   void registerMessageQueueFunctions();
   void registerMutexFunctions();
   void registerSpinLockFunctions();
   void registerSystemInfoExports();
   void registerTimeExports();

   void initialiseAlarmStaticData();
   void initialiseAllocatorStaticData();
   void initialiseDefaultHeapStaticData();
   void initialiseEventStaticData();
   void initialiseMemHeapStaticData();
   void initialiseSchedulerStaticData();
   void initialiseSystemInfoStaticData();

   void initialiseClock();

private:
   void CafeInit();
};

} // namespace cafe::coreinit

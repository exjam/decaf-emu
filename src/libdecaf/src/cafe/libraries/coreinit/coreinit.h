#pragma once
#include "cafe/libraries/cafe_hle_library.h"
#include <libcpu/be2_struct.h>
#include <memory>

namespace cafe::coreinit
{

struct StaticAlarmData;
struct StaticAllocatorData;
struct StaticAppIoData;
struct StaticBspData;
struct StaticClipboardData;
struct StaticDefaultHeapData;
struct StaticDeviceData;
struct StaticDriverData;
struct StaticEventData;
struct StaticImData;
struct StaticIpcDriverData;
struct StaticMemHeapData;
struct StaticSchedulerData;
struct StaticSystemInfoData;
struct StaticThreadData;

struct LibraryStaticData
{
   virt_ptr<StaticAlarmData> alarmData;
   virt_ptr<StaticAllocatorData> allocatorData;
   virt_ptr<StaticAppIoData> appIoData;
   virt_ptr<StaticBspData> bspData;
   virt_ptr<StaticClipboardData> clipboardData;
   virt_ptr<StaticDefaultHeapData> defaultHeapData;
   virt_ptr<StaticDeviceData> deviceData;
   virt_ptr<StaticDriverData> driverData;
   virt_ptr<StaticEventData> eventData;
   virt_ptr<StaticImData> imData;
   virt_ptr<StaticIpcDriverData> ipcDriverData;
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
   void registerBspFunctions();
   void registerCacheFunctions();
   void registerClipboardExports();
   void registerCoreFunctions();
   void registerCoroutineFunctions();
   void registerDeviceFunctions();
   void registerDriverFunctions();
   void registerEventFunctions();
   void registerFastMutexFunctions();
   void registerFiberExports();
   void registerImExports();
   void registerInterruptExports();
   void registerIosFunctions();
   void registerIpcBufPoolFunctions();
   void registerIpcDriverFunctions();
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
   void registerRendezvousExports();
   void registerSemaphoreFunctions();
   void registerSpinLockFunctions();
   void registerSystemInfoExports();
   void registerTaskQueueExports();
   void registerTimeExports();

   void initialiseAlarmStaticData();
   void initialiseAllocatorStaticData();
   void initialiseAppIoStaticData();
   void initialiseBspStaticData();
   void initialiseClipboardStaticData();
   void initialiseDefaultHeapStaticData();
   void initialiseDeviceStaticData();
   void initialiseDriverStaticData();
   void initialiseEventStaticData();
   void initialiseImStaticData();
   void initialiseIpcDriverStaticData();
   void initialiseMemHeapStaticData();
   void initialiseSchedulerStaticData();
   void initialiseSystemInfoStaticData();

   void initialiseClock();

   virt_ptr<void>
   allocStaticData(uint32_t size, uint32_t align);

   template<typename Type>
   virt_ptr<Type>
   allocStaticData()
   {
      auto buffer = allocStaticData(sizeof(Type), alignof(Type));
      auto data = virt_cast<Type>(buffer);
      std::uninitialized_default_construct_n(data.getRawPointer(), 1);
      return data;
   }

private:
   void CafeInit();
};

} // namespace cafe::coreinit

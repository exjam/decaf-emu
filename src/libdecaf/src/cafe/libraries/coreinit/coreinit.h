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
struct StaticDynLoadData;
struct StaticEventData;
struct StaticFsData;
struct StaticFsClientData;
struct StaticFsCmdData;
struct StaticFsCmdBlockData;
struct StaticFsDriverData;
struct StaticFsStateMachineData;
struct StaticHandleData;
struct StaticImData;
struct StaticIpcDriverData;
struct StaticLockedCacheData;
struct StaticMcpData;
struct StaticMemHeapData;
struct StaticSchedulerData;
struct StaticSystemHeapData;
struct StaticSystemInfoData;
struct StaticThreadData;
struct StaticUserConfigData;

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
   virt_ptr<StaticDynLoadData> dynLoadData;
   virt_ptr<StaticEventData> eventData;
   virt_ptr<StaticFsData> fsData;
   virt_ptr<StaticFsClientData> fsClientData;
   virt_ptr<StaticFsCmdData> fsCmdData;
   virt_ptr<StaticFsCmdBlockData> fsCmdBlockData;
   virt_ptr<StaticFsDriverData> fsDriverData;
   virt_ptr<StaticFsStateMachineData> fsStateMachineData;
   virt_ptr<StaticHandleData> handleData;
   virt_ptr<StaticImData> imData;
   virt_ptr<StaticIpcDriverData> ipcDriverData;
   virt_ptr<StaticLockedCacheData> lockedCacheData;
   virt_ptr<StaticMcpData> mcpData;
   virt_ptr<StaticMemHeapData> memHeapData;
   virt_ptr<StaticSchedulerData> schedulerData;
   virt_ptr<StaticSystemHeapData> systemHeapData;
   virt_ptr<StaticSystemInfoData> systemInfoData;
   virt_ptr<StaticThreadData> threadData;
   virt_ptr<StaticUserConfigData> userConfigData;
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
   void registerDynLoadExports();
   void registerEventFunctions();
   void registerFastMutexFunctions();
   void registerFiberExports();
   void registerFsFunctions();
   void registerFsClientFunctions();
   void registerFsCmdFunctions();
   void registerFsCmdBlockFunctions();
   void registerFsDriverFunctions();
   void registerFsStatemachineFunctions();
   void registerFsaFunctions();
   void registerFsaShimFunctions();
   void registerHandleExports();
   void registerImExports();
   void registerInterruptExports();
   void registerIosFunctions();
   void registerIpcBufPoolFunctions();
   void registerIpcDriverFunctions();
   void registerLockedCacheFunctions();
   void registerMcpFunctions();
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
   void registerSystemHeapExports();
   void registerTaskQueueExports();
   void registerTimeExports();
   void registerUserConfigFunctions();

   void initialiseAlarmStaticData();
   void initialiseAllocatorStaticData();
   void initialiseAppIoStaticData();
   void initialiseBspStaticData();
   void initialiseClipboardStaticData();
   void initialiseDefaultHeapStaticData();
   void initialiseDeviceStaticData();
   void initialiseDriverStaticData();
   void initialiseDynLoadStaticData();
   void initialiseEventStaticData();
   void initialiseFsStaticData();
   void initialiseFsClientStaticData();
   void initialiseFsCmdStaticData();
   void initialiseFsCmdBlockStaticData();
   void initialiseFsDriverStaticData();
   void initialiseFsStateMachineStaticData();
   void initialiseHandleStaticData();
   void initialiseImStaticData();
   void initialiseIpcDriverStaticData();
   void initialiseLockedCacheStaticData();
   void initialiseMemHeapStaticData();
   void initialiseSchedulerStaticData();
   void initialiseSystemInfoStaticData();
   void initialiseSystemHeapStaticData();
   void initialiseUserConfigStaticData();

   void initialiseClock();

   virt_ptr<void>
   allocStaticData(uint32_t size, uint32_t align);

   template<typename Type>
   virt_ptr<Type>
   allocStaticData()
   {
      auto buffer = allocStaticData(sizeof(Type), alignof(Type));
      auto data = virt_cast<Type *>(buffer);
      std::uninitialized_default_construct_n(data.getRawPointer(), 1);
      return data;
   }

private:
   void CafeInit();
};

} // namespace cafe::coreinit

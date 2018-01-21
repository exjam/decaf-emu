#include "coreinit.h"
#include "coreinit_appio.h"
#include "coreinit_core.h"
#include "coreinit_thread.h"
#include "coreinit_messagequeue.h"
#include "cafe/cafe_stackobject.h"

namespace cafe::coreinit
{

constexpr auto MessagesPerCore = 256u;
constexpr auto AppIoThreadStackSize = 0x2000u;

struct StaticAppIoData
{
   struct PerCoreData
   {
      be2_struct<OSThread> thread;
      be2_struct<OSMessageQueue> queue;
      be2_array<OSMessage, MessagesPerCore> messages;
      be2_array<uint8_t, AppIoThreadStackSize> stack;
   };

   be2_array<char, 16> threadName0 = "I/O Thread 0";
   be2_array<char, 16> threadName1 = "I/O Thread 1";
   be2_array<char, 16> threadName2 = "I/O Thread 2";
   be2_val<OSThreadEntryPointFn> threadEntry;

   be2_array<PerCoreData, CoreCount> perCoreData;
};

static virt_ptr<StaticAppIoData>
getAppIoData()
{
   return Library::getStaticData()->appIoData;
}

virt_ptr<OSMessageQueue>
OSGetDefaultAppIOQueue()
{
   return virt_addrof(getAppIoData()->perCoreData[OSGetCoreId()].queue);
}

static uint32_t
appIoThreadEntry(uint32_t coreId,
                 virt_ptr<void> arg2)
{
   StackObject<OSMessage> msg;
   auto appIoData = getAppIoData();
   auto coreData = virt_addrof(appIoData->perCoreData[coreId]);
   auto queue = virt_addrof(coreData->queue);
   OSInitMessageQueue(queue,
                      virt_addrof(coreData->messages),
                      coreData->messages.size());

   while (true) {
      OSReceiveMessage(queue, msg, OSMessageFlags::Blocking);

      auto funcType = static_cast<OSFunctionType>(msg->args[2].value());
      switch (funcType) {
      case OSFunctionType::FsaCmdAsync:
      {
         auto result = FSAGetAsyncResult(msg);
         if (result->userCallback) {
            result->userCallback(result->error,
                                 result->command,
                                 result->request,
                                 result->response,
                                 result->userContext);
         }
         break;
      }
      case OSFunctionType::FsCmdAsync:
      {
         auto result = FSGetAsyncResult(msg);
         if (result->asyncData.userCallback) {
            result->asyncData.userCallback(result->client,
                                           result->block,
                                           result->status,
                                           result->asyncData.userContext);
         }
         break;
      }
      case OSFunctionType::FsCmdHandler:
      {
         internal::fsCmdBlockHandleResult(virt_cast<FSCmdBlockBody *>(msg->message));
         break;
      }
      default:
         decaf_abort(fmt::format("Unimplemented OSFunctionType {}", funcType));
      }
   }
}

namespace internal
{

void
startAppIoThreads()
{
   auto appIoData = getAppIoData();

   for (auto i = 0u; i < appIoData->perCoreData.size(); ++i) {
      auto &coreData = appIoData->perCoreData[i];
      auto thread = virt_addrof(coreData.thread);
      auto stack = virt_addrof(coreData.stack);

      OSCreateThread(thread,
                     appIoData->threadEntry,
                     i,
                     nullptr,
                     stack + coreData.stack.size(),
                     coreData.stack.size(),
                     -1,
                     static_cast<OSThreadAttributes>(1 << i));

      switch (i) {
      case 0:
         OSSetThreadName(thread, virt_addrof(appIoData->threadName0));
         break;
      case 1:
         OSSetThreadName(thread, virt_addrof(appIoData->threadName1));
         break;
      case 2:
         OSSetThreadName(thread, virt_addrof(appIoData->threadName2));
         break;
      }

      OSResumeThread(thread);
   }
}

} // namespace internal

void
Library::initialiseAppIoStaticData()
{
   auto appIoData = allocStaticData<StaticAppIoData>();
   Library::getStaticData()->appIoData = appIoData;

   appIoData->threadEntry = GetInternalFunctionAddress(appIoThreadEntry);
}

void
Library::registerAppIoExports()
{
   RegisterFunctionExport(OSGetDefaultAppIOQueue);
   RegisterFunctionInternal(appIoThreadEntry);
}

} // namespace cafe::coreinit


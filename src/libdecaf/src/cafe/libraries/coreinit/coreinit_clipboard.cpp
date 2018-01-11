#include "coreinit.h"
#include "coreinit_clipboard.h"
#include "coreinit_memory.h"
#include "cafe/cafe_stackobject.h"

#include <vector>

namespace cafe::coreinit
{

constexpr auto MaxClipboardSize = 0x800u;

struct ClipboardSaveData
{
   be2_val<uint32_t> size;
   UNKNOWN(0xC);
   be2_array<uint8_t, MaxClipboardSize> buffer;
};

struct StaticClipboardData
{
   be2_struct<ClipboardSaveData> clipboard;
};

static virt_ptr<StaticClipboardData>
getClipboardData()
{
   return Library::getStaticData()->clipboardData;
}

BOOL
OSCopyFromClipboard(virt_ptr<void> buffer,
                    virt_ptr<uint32_t> size)
{
   auto clipboardData = getClipboardData();

   if (!size) {
      return FALSE;
   }

   if (buffer && *size == 0) {
      return FALSE;
   }

   if (!OSGetForegroundBucket(nullptr, nullptr)) {
      // Can only use clipboard when application is in foreground.
      return FALSE;
   }

   // TODO: if (!OSDriver_CopyFromSaveArea(0xA, virt_addrof(clipboardData->clipboard), sizeof(ClipboardSaveData))) { return false; }

   if (buffer && clipboardData->clipboard.size) {
      std::memcpy(buffer.getRawPointer(),
                  virt_addrof(clipboardData->clipboard.buffer).getRawPointer(),
                  std::min<size_t>(clipboardData->clipboard.size, *size));
   }

   *size = clipboardData->clipboard.size;
   return TRUE;
}

BOOL
OSCopyToClipboard(virt_ptr<const void> buffer,
                  uint32_t size)
{
   auto clipboardData = getClipboardData();

   if (!OSGetForegroundBucket(nullptr, nullptr)) {
      // Can only use clipboard when application is in foreground.
      return FALSE;
   }

   if (size > MaxClipboardSize) {
      return FALSE;
   }

   if (buffer) {
      clipboardData->clipboard.size = size;

      std::memcpy(virt_addrof(clipboardData->clipboard.buffer).getRawPointer(),
                  buffer.getRawPointer(),
                  size);
   } else {
      if (size != 0) {
         return FALSE;
      }

      clipboardData->clipboard.size = 0;
   }

   // TODO: return OSDriver_CopyToSaveArea(0xA, virt_addrof(clipboardData->clipboard), sizeof(ClipboardSaveData)));
   return TRUE;
}

void
Library::registerClipboardExports()
{
   RegisterFunctionExport(OSCopyFromClipboard);
   RegisterFunctionExport(OSCopyToClipboard);
}

} // namespace cafe::coreinit

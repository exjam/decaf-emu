#include "packet.h"
#include "console.h"

#include <coreinit/baseheap.h>
#include <coreinit/expandedheap.h>

#include <string.h>
#include <whb/log.h>

#define INITIAL_WRITE_PACKET_SIZE 1024

uint32_t
pakReadUint32(PacketReader *packet)
{
   uint32_t value;
   memcpy(&value, packet->data + packet->pos, sizeof(uint32_t));
   packet->pos += sizeof(uint32_t);

   if (packet->pos > packet->dataLength) {
      WHBLogPrintf("Read past end of packet! pos: %d length: %d", packet->pos, packet->dataLength);
   }

   return value;
}

uint64_t
pakReadUint64(PacketReader *packet)
{
   uint64_t value;
   memcpy(&value, packet->data + packet->pos, sizeof(uint64_t));
   packet->pos += sizeof(uint64_t);

   if (packet->pos > packet->dataLength) {
      WHBLogPrintf("Read past end of packet! pos: %d length: %d", packet->pos, packet->dataLength);
   }

   return value;
}

int32_t
pakReadInt32(PacketReader *packet)
{
   return (int32_t)pakReadUint32(packet);
}

int64_t
pakReadInt64(PacketReader *packet)
{
   return (int64_t)pakReadUint32(packet);
}

void *
pakReadPointer(PacketReader *packet)
{
   return (void *)pakReadUint32(packet);
}

const uint8_t *
pakReadData(PacketReader *packet, uint32_t *length)
{
   const uint8_t *data;
   *length = pakReadUint32(packet);
   data = packet->data + packet->pos;
   packet->pos += *length;

   if (packet->pos > packet->dataLength) {
      WHBLogPrintf("Read past end of packet! pos: %d length: %d", packet->pos, packet->dataLength);
   }

   return data;
}

const char *
pakReadString(PacketReader *packet)
{
   uint32_t length = pakReadUint32(packet);
   const uint8_t *str = packet->data + packet->pos;
   packet->pos += length;

   if (packet->pos > packet->dataLength) {
      WHBLogPrintf("Read past end of packet! pos: %d length: %d", packet->pos, packet->dataLength);
   }

   return (const char *)str;
}

void
pakWriteAlloc(PacketWriter *packet, uint32_t command)
{
   MEMExpandedHeap *mem2 = (MEMExpandedHeap *)MEMGetBaseHeapHandle(MEM_BASE_HEAP_MEM2);

   packet->command = command;
   packet->pos = 0;
   packet->bufferSize = INITIAL_WRITE_PACKET_SIZE;
   packet->buffer = (uint8_t *)MEMAllocFromExpHeapEx(mem2, packet->bufferSize, 16);

   pakWriteUint32(packet, 8);
   pakWriteUint32(packet, command);
}

void
pakWriteFree(PacketWriter *packet)
{
   MEMExpandedHeap *mem2 = (MEMExpandedHeap *)MEMGetBaseHeapHandle(MEM_BASE_HEAP_MEM2);
   MEMFreeToExpHeap(mem2, packet->buffer);
   packet->buffer = NULL;
}

static void
pakWriteIncreaseSize(PacketWriter *packet, uint32_t size)
{
   MEMExpandedHeap *mem2 = (MEMExpandedHeap *)MEMGetBaseHeapHandle(MEM_BASE_HEAP_MEM2);
   uint32_t newSize = packet->pos + size;

   if (newSize > packet->bufferSize) {
      uint32_t newBufferSize = packet->bufferSize;

      while (newSize > newBufferSize) {
         newBufferSize += INITIAL_WRITE_PACKET_SIZE;
      }

      uint8_t *newBuffer = (uint8_t *)MEMAllocFromExpHeapEx(mem2, newBufferSize, 16);
      memcpy(newBuffer, packet->buffer, packet->bufferSize);
      MEMFreeToExpHeap(mem2, packet->buffer);
      packet->buffer = newBuffer;
      packet->bufferSize = newBufferSize;
   }

   // Update size at word 0
   memcpy(packet->buffer, &newSize, sizeof(uint32_t));
}

void
pakWriteUint32(PacketWriter *packet, uint32_t value)
{
   uint8_t *dst;
   pakWriteIncreaseSize(packet, sizeof(uint32_t));

   dst = packet->buffer + packet->pos;
   memcpy(dst, &value, sizeof(uint32_t));
   packet->pos += sizeof(uint32_t);
}

void
pakWriteUint64(PacketWriter *packet, uint64_t value)
{
   uint8_t *dst;
   pakWriteIncreaseSize(packet, sizeof(uint64_t));

   dst = packet->buffer + packet->pos;
   memcpy(dst, &value, sizeof(uint64_t));
   packet->pos += sizeof(uint64_t);
}

void
pakWriteInt32(PacketWriter *packet, int32_t value)
{
   pakWriteUint32(packet, (uint32_t)value);
}

void
pakWriteInt64(PacketWriter *packet, int64_t value)
{
   pakWriteUint64(packet, (uint64_t)value);
}

void
pakWritePointer(PacketWriter *packet, void *value)
{
   pakWriteUint32(packet, (uint32_t)value);
}

void
pakWriteString(PacketWriter *packet, const char *str)
{
   uint32_t length = str ? strlen(str) : 0;
   pakWriteData(packet, (const uint8_t *)str, length + 1);
}

void
pakWriteData(PacketWriter *packet, const void *data, uint32_t length)
{
   uint8_t *dst;
   pakWriteUint32(packet, length);

   if (data && length > 0) {
      pakWriteIncreaseSize(packet, length);

      dst = packet->buffer + packet->pos;
      memcpy(dst, data, length);
      packet->pos += length;
   }
}

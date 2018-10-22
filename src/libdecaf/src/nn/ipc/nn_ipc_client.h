#pragma optimize("", off)
#pragma once
#include "nn_ipc_transferablebufferallocator.h"

#include "cafe/libraries/coreinit/coreinit_ios.h"
#include "nn/nn_result.h"
#include "nn/acp/nn_acp_miscservice.h"
#include "nn/ios/nn_ios_error.h"
#include "nn/ipc/nn_ipc_format.h"
#include "nn/ipc/nn_ipc_managedbuffer.h"

#include <array>
#include <libcpu/be2_struct.h>

namespace nn::ipc
{

namespace detail
{

template<int, int, typename... Types>
struct ClientIpcDataHelper;

struct ManagedBufferInfo
{
   virt_ptr<void> ipcBuffer = nullptr;

   virt_ptr<void> userBuffer;
   uint32_t userBufferSize;

   virt_ptr<void> unalignedBeforeBuffer;
   uint32_t unalignedBeforeBufferSize;

   virt_ptr<void> alignedBuffer;
   uint32_t alignedBufferSize;

   virt_ptr<void> unalignedAfterBuffer;
   uint32_t unalignedAfterBufferSize;

   bool output;
};

/*
IOCTLV order:
vec[0] = response buffer
... user output vecs ...
vec[numVecIn] = request buffer
... user input vecs ...
*/

template<typename... Types>
struct ManagedBufferCount;

template<>
struct ManagedBufferCount<>
{
   static constexpr auto Input = 0;
   static constexpr auto Output = 0;
};

template<typename T, typename... Types>
struct ManagedBufferCount<T, Types...>
{
   static constexpr auto Input = 0 + ManagedBufferCount<Types...>::Input;
   static constexpr auto Output = 0 + ManagedBufferCount<Types...>::Output;
};

template<typename T, typename... Types>
struct ManagedBufferCount<InBuffer<T>, Types...>
{
   static constexpr auto Input = 1 + ManagedBufferCount<Types...>::Input;
   static constexpr auto Output = 0 + ManagedBufferCount<Types...>::Output;
};

template<typename T, typename... Types>
struct ManagedBufferCount<InOutBuffer<T>, Types...>
{
   static constexpr auto Input = 1 + ManagedBufferCount<Types...>::Input;
   static constexpr auto Output = 1 + ManagedBufferCount<Types...>::Output;
};

template<typename T, typename... Types>
struct ManagedBufferCount<OutBuffer<T>, Types...>
{
   static constexpr auto Input = 0 + ManagedBufferCount<Types...>::Input;
   static constexpr auto Output = 1 + ManagedBufferCount<Types...>::Output;
};

template<int ServiceId, int CommandId, typename... ParameterTypes, typename... ResponseTypes>
struct ClientIpcDataHelper<ServiceId, CommandId, std::tuple<ParameterTypes...>, std::tuple<ResponseTypes...>>
{
   static constexpr auto NumInputBuffers = ManagedBufferCount<ParameterTypes...>::Input;
   static constexpr auto NumOutputBuffers = ManagedBufferCount<ParameterTypes...>::Output;

   ClientIpcDataHelper(virt_ptr<TransferableBufferAllocator> allocator) :
      mAllocator(allocator)
   {
      mVecsBuffer = virt_cast<cafe::coreinit::IOSVec *>(mAllocator->Allocate(128));
      mRequestBuffer = mAllocator->Allocate(128);
      mResponseBuffer = mAllocator->Allocate(128);

      std::memset(mVecsBuffer.get(), 0, 128);
      std::memset(mRequestBuffer.get(), 0, 128);
      std::memset(mResponseBuffer.get(), 0, 128);

      auto request = virt_cast<RequestHeader *>(mRequestBuffer);
      request->unk0x00 = 1u;
      request->command = static_cast<uint32_t>(CommandId);
      request->unk0x08 = 0u;
      request->service = static_cast<uint32_t>(ServiceId);

      mVecsBuffer[0].vaddr = virt_cast<virt_addr>(mResponseBuffer);
      mVecsBuffer[0].len = 128u;

      mVecsBuffer[1 + NumOutputBuffers].vaddr = virt_cast<virt_addr>(mRequestBuffer);
      mVecsBuffer[1 + NumOutputBuffers].len = 128u;
   }

   ClientIpcDataHelper()
   {
      if (mVecsBuffer) {
         mAllocator->Deallocate(mVecsBuffer);
      }

      if (mRequestBuffer) {
         mAllocator->Deallocate(mRequestBuffer);
      }

      if (mResponseBuffer) {
         mAllocator->Deallocate(mResponseBuffer);
      }

      for (auto &ioBuffer : mInOutBuffers) {
         if (ioBuffer.ipcBuffer) {
            mAllocator->Deallocate(ioBuffer.ipcBuffer);
         }
      }
   }

   template<typename Type>
   void WriteParameter(size_t &offset, int inputVecIdx, int outputVecIdx, Type value)
   {
      auto ptr = virt_cast<Type *>(virt_cast<virt_addr>(mRequestBuffer) + offset);
      *ptr = value;
      offset += sizeof(Type);
   }

   void WriteParameter(size_t &offset, int inputVecIdx, int outputVecIdx, ipc::ManagedBuffer userBuffer)
   {
      // The user buffer pointer is not guaranteed to be aligned so we must
      // split the buffer by separately reading / writing the unaligned data at
      // the start and end of the user buffer.
      auto &ioBuffer = mInOutBuffers[(inputVecIdx + outputVecIdx) / 2];
      ioBuffer.ipcBuffer = mAllocator->Allocate(256);
      ioBuffer.userBuffer = userBuffer.ptr;
      ioBuffer.userBufferSize = userBuffer.size;
      ioBuffer.output = userBuffer.output;

      auto midPoint = virt_cast<virt_addr>(ioBuffer.ipcBuffer + 64);

      auto unalignedStart = virt_cast<virt_addr>(userBuffer.ptr);
      auto unalignedEnd = unalignedStart + userBuffer.size;

      auto alignedStart = align_up(unalignedStart, 64);
      auto alignedEnd = align_down(unalignedEnd, 64);

      ioBuffer.alignedBuffer = virt_cast<void>(alignedStart);
      ioBuffer.alignedBufferSize = static_cast<uint32_t>(alignedEnd - alignedStart);

      ioBuffer.unalignedBeforeBufferSize = static_cast<uint32_t>(alignedStart - unalignedStart);
      ioBuffer.unalignedBeforeBuffer = virt_cast<void *>(virt_cast<virt_addr>(ioBuffer.ipcBuffer + 64 - ioBuffer.unalignedBeforeBufferSize));

      ioBuffer.unalignedAfterBufferSize = static_cast<uint32_t>(unalignedEnd - alignedEnd);
      ioBuffer.unalignedAfterBuffer = virt_cast<void *>(virt_cast<virt_addr>(ioBuffer.ipcBuffer + 64));

      if (userBuffer.input) {
         // Copy the unaligned buffer input
         std::memcpy(ioBuffer.unalignedBeforeBuffer.get(),
                     virt_cast<void *>(unalignedStart),
                     ioBuffer.unalignedBeforeBufferSize);

         std::memcpy(ioBuffer.unalignedAfterBuffer.get(),
                     virt_cast<void *>(unalignedEnd - ioBuffer.unalignedAfterBufferSize),
                     ioBuffer.unalignedAfterBufferSize);
      }

      if (userBuffer.output) {
         // Save this for later so we can copy the unaligned buffer output after
         // we receive response from server
         mOutputBuffers[outputVecIdx / 2] = ioBuffer;
      }

      // Calculate our ioctlv vecs indices
      auto alignedBufferIndex = uint8_t { 0 };
      auto unalignedBufferIndex = uint8_t { 0 };
      auto bufferIndexOffset = uint8_t { 0 };

      if (userBuffer.input) {
         alignedBufferIndex = static_cast<uint8_t>(inputVecIdx++);
         unalignedBufferIndex = static_cast<uint8_t>(inputVecIdx++);
         bufferIndexOffset = 2 + NumOutputBuffers;
      } else {
         alignedBufferIndex = static_cast<uint8_t>(outputVecIdx++);
         unalignedBufferIndex = static_cast<uint8_t>(outputVecIdx++);
         bufferIndexOffset = 1;
      }

      // Update our ioctlv vecs buffer
      auto alignedBufferVec = &mVecsBuffer[bufferIndexOffset + alignedBufferIndex];
      auto unalignedBufferVec = &mVecsBuffer[bufferIndexOffset + unalignedBufferIndex];
      alignedBufferVec.vaddr = virt_cast<virt_addr>(ioBuffer.alignedBuffer);
      alignedBufferVec.size = ioBuffer.alignedBufferSize;

      if (ioBuffer.unalignedBeforeBufferSize + ioBuffer.unalignedAfterBufferSize) {
         unalignedBufferVec.vaddr = virt_cast<virt_addr>(ioBuffer.unalignedBeforeBuffer);
         unalignedBufferVec.size = ioBuffer.unalignedBeforeBufferSize + ioBuffer.unalignedAfterBufferSize;
      } else {
         unalignedBufferVec.vaddr = virt_addr { 0u };
         unalignedBufferVec.size = 0u;
      }

      // Serialise the buffer info to the request
      auto managedBuffer = virt_cast<ManagedBufferParameter *>(virt_cast<virt_addr>(mRequestBuffer) + offset);
      managedBuffer->alignedBufferSize = ioBuffer.alignedBufferSize;
      managedBuffer->unalignedBeforeBufferSize = static_cast<uint8_t>(ioBuffer.unalignedBeforeBufferSize);
      managedBuffer->unalignedAfterBufferSize = static_cast<uint8_t>(ioBuffer.unalignedAfterBufferSize);
      managedBuffer->alignedBufferIndex = alignedBufferIndex;
      managedBuffer->unalignedBufferIndex = unalignedBufferIndex;
      offset += 8;
   }

   void SetParameters(ParameterTypes... params)
   {
      auto offset = sizeof(RequestHeader);
      auto inputVecIdx = 0;
      auto outputVecIdx = 0;
      (WriteParameter(offset, inputVecIdx, outputVecIdx, params), ...);
   }

   template<typename Type>
   void ReadResponseValue(size_t &offset, Type &value)
   {
      auto ptr = virt_cast<Type *>(virt_cast<virt_addr>(mResponseBuffer) + offset);
      value = *ptr;
      offset += sizeof(Type);
   }

   nn::Result ReadResponse(ResponseTypes &... responses)
   {
      auto header = virt_cast<ResponseHeader *>(mResponseBuffer);

      // Read response values
      auto offset = sizeof(ResponseHeader);

      // Read unaligned output buffer data
      for (auto &ioBuffer : mInOutBuffers) {
         if (!ioBuffer.output) {
            continue;
         }

         std::memcpy(ioBuffer.userBuffer.get(),
                     ioBuffer.unalignedBeforeBuffer.get(),
                     ioBuffer.unalignedBeforeBufferSize);

         auto userAfterBufferAddr =
            virt_cast<virt_addr>(ioBuffer.userBuffer) + ioBuffer.userBufferSize - ioBuffer.unalignedAfterBufferSize;

         std::memcpy(virt_cast<void *>(userAfterBufferAddr).get(),
                     ioBuffer.unalignedAfterBuffer.get(),
                     ioBuffer.unalignedAfterBufferSize);
      }

      (ReadResponseValue(offset, responses),  ...);
      return nn::Result { static_cast<uint32_t>(static_cast<int32_t>(header->result)) };
   }

   virt_ptr<TransferableBufferAllocator> mAllocator;
   virt_ptr<void> mRequestBuffer;
   virt_ptr<void> mResponseBuffer;

   virt_ptr<cafe::coreinit::IOSVec> mVecsBuffer;

   // State for serialisation
   uint32_t mInputVecIdx;
   uint32_t mOutputVecIdx;

   std::array<ManagedBufferInfo, NumOutputBuffers + NumInputBuffers> mInOutBuffers;
};

} // namespace detail

template<typename CommandType>
struct ClientIpcData;

template<typename CommandType>
struct ClientIpcData : detail::ClientIpcDataHelper<CommandType::service, CommandType::command, typename CommandType::parameters, typename CommandType::response>
{
   ClientIpcData(virt_ptr<TransferableBufferAllocator> allocator) :
      detail::ClientIpcDataHelper<CommandType::service, CommandType::command, typename CommandType::parameters, typename CommandType::response>(allocator)
   {
   }
};

struct Client
{
   nn::Result Initialize(virt_ptr<const char> device)
   {
      auto error = cafe::coreinit::IOS_Open(device, cafe::coreinit::IOSOpenMode::None);
      if (error < 0) {
         return nn::ios::convertError(error);
      }

      mHandle = static_cast<cafe::coreinit::IOSHandle>(error);
      return nn::ios::ResultOK;
   }

   template<typename CommandType>
   nn::Result SendSyncRequest(ClientIpcData<CommandType> &command)
   {
      auto error = cafe::coreinit::IOS_Ioctlv(
         mHandle,
         0,
         1 + ClientIpcData<CommandType>::NumOutputBuffers,
         1 + ClientIpcData<CommandType>::NumInputBuffers,
         command.mVecsBuffer);

      if (error < 0) {
         return nn::ios::convertError(error);
      }

      return nn::ResultSuccess;
   }

    cafe::coreinit::IOSHandle mHandle;
};

} // namespace nn::ipc

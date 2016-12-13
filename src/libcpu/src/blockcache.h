#pragma once
#include "mem.h"
#include <cstdint>
#include <atomic>
#include <array>
#include <unordered_map>
#include <assert.h>
#include <common/decaf_assert.h>
#include <thread>
#include <gsl/gsl>
#include <mutex>

#include <common/align.h>
#include <common/platform.h>
#include <common/platform_memory.h>
#include <Windows.h>

struct CodeBlock
{
   uint32_t address;
   void *code;
   uint32_t codeSize;

#ifdef PLATFORM_WINDOWS
   static constexpr size_t MaxUnwindInfoSize = 4 + 2 * 30 + 8;
   std::array<uint8_t, MaxUnwindInfoSize> unwindInfo;
   uint32_t unwindSize;
   RUNTIME_FUNCTION rtlFuncTable;
#endif

   struct
   {
      std::atomic<uint64_t> count;
      std::atomic<uint64_t> time;
   } profileData;
};

using CodeBlockIndex = int32_t;

/**
 * Responsibilities:
 *
 * 1. Map guest address to host address.
 * 2. Allocate executable host memory.
 * 3. Allocate and populate unwind information.
 */
class CodeCache
{
   struct FrameAllocator
   {
      // Memory flags
      platform::ProtectFlags flags;

      //! Base address
      uintptr_t baseAddress;

      //! Amount of data allocated so far.
      std::atomic<size_t> allocated;

      //! Amount of memory committed
      std::atomic<size_t> committed;

      //! Amount of memory reserved
      size_t reserved;

      size_t growthSize;
      std::mutex mutex;
   };

   static constexpr size_t Level1Size = 0x100;
   static constexpr size_t Level2Size = 0x100;
   static constexpr size_t Level3Size = 0x4000;

public:
   static constexpr CodeBlockIndex BlockUncompiled = -1;
   static constexpr CodeBlockIndex BlockCompiling = -2;

   CodeCache();
   ~CodeCache();

   size_t getCodeCacheSize();

   bool initialise(size_t codeSize, size_t dataSize);

   void clear();

   void free();

   void setBlockIndex(uint32_t address, CodeBlockIndex index);

   CodeBlock *getBlockByAddress(uint32_t address);

   CodeBlock *getBlockByIndex(CodeBlockIndex index);

   std::atomic<CodeBlockIndex> *getIndexPointer(uint32_t address);

   CodeBlockIndex getIndex(uint32_t address);

   CodeBlockIndex getIndex(CodeBlock *block);

   CodeBlock *registerCodeBlock(uint32_t address, void *code, size_t size, void *unwindInfo, size_t unwindSize);

   gsl::span<CodeBlock> getCompiledCodeBlocks();

private:
   uintptr_t allocate(FrameAllocator &allocator, size_t size, size_t alignment);

private:
   size_t mReserveAddress;
   size_t mReserveSize;
   FrameAllocator mCodeAllocator;
   FrameAllocator mDataAllocator;

   std::atomic<std::atomic<std::atomic<CodeBlockIndex> *> *> *mFastIndex;
};


#pragma once
#include "mem.h"
#include "state.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <utility>
#include <gsl.h>

struct Tracer;

namespace cpu
{

enum ExceptionFlags : uint32_t
{
   SystemResetException          = 1 << 0,
   MachineCheckException         = 1 << 1,
   DSIException                  = 1 << 2,
   ISIException                  = 1 << 3,
   ExternalInterruptException    = 1 << 4,
   AlignmentException            = 1 << 5,
   ProgramException              = 1 << 6,
   FloatingPointException        = 1 << 7,
   DecrementerException          = 1 << 8,
   // Reserved
   // Reserved
   SystemCallException           = 1 << 11,
   TraceException                = 1 << 12,
   // Reserved
   PerformanceMonitorException   = 1 << 14,
   // Reserved
   // Reserved
   // Reserved
   BreakpointException           = 1 << 18,
   // Reserved
   // Reserved
   // Reserved
   // Reserved
   // Reserved

   // Users of libcpu should specify their own exceptions between bit 24 and 31
   UserFirstException            = 1 << 24,
   UserLastException             = 1 << 31,
   UserFirstExceptionBit         = 24,
   UserLastExceptionBit          = 31,
};

const uint32_t GPU_RETIRE_INTERRUPT = 1 << 4;
const uint32_t GPU_FLIP_INTERRUPT = 1 << 5;
const uint32_t IPC_INTERRUPT = 1 << 6;
const uint32_t INTERRUPT_MASK = 0xFFFFFFFF;
const uint32_t NONMASKABLE_INTERRUPTS = SRESET_INTERRUPT;

const uint32_t InvalidCoreId = 0xFF;

enum class jit_mode {
   disabled,
   enabled,
   verify
};

static const uint32_t CALLBACK_ADDR = 0xFBADCDE0;

using EntrypointHandler = std::function<void(Core *core)>;
using InterruptHandler = void (*)(Core *core, ExceptionFlags interrupt_flags);
using SegfaultHandler = void(*)(Core *core, uint32_t address);
using IllInstHandler = void(*)(Core *core);
using BranchTraceHandler = void(*)(Core *core, uint32_t target);
using KernelCallFunction = void(*)(Core *core, void *userData);

struct KernelCallEntry
{
   KernelCallFunction func;
   void *user_data;
};

void
initialise();

void
clearInstructionCache();

void
invalidateInstructionCache(uint32_t address,
                           uint32_t size);

void
addJitReadOnlyRange(ppcaddr_t address, uint32_t size);

void
setCoreEntrypointHandler(EntrypointHandler handler);

void
setInterruptHandler(InterruptHandler handler);

void
setSegfaultHandler(SegfaultHandler handler);

void
setIllInstHandler(IllInstHandler handler);

void
setBranchTraceHandler(BranchTraceHandler handler);

uint32_t
registerKernelCall(const KernelCallEntry &entry);

void
start();

void
join();

void
halt();

std::chrono::steady_clock::time_point
tbToTimePoint(uint64_t ticks);

using Tracer = ::Tracer;

Tracer *
allocTracer(size_t size);

void
freeTracer(Tracer *tracer);

void
interrupt(int core_idx,
          ExceptionFlags flags);

namespace this_core
{

void
setTracer(Tracer *tracer);

void
resume();

void
executeSub();

void
checkInterrupts();

void
waitForInterrupt();

void
waitNextInterrupt(std::chrono::steady_clock::time_point until);

ExceptionFlags
interruptMask();

ExceptionFlags
setInterruptMask(ExceptionFlags mask);

void
clearInterrupt(ExceptionFlags flags);

void
setNextAlarm(std::chrono::steady_clock::time_point alarm_time);

cpu::Core *
state();

static uint32_t id()
{
   auto core = state();

   if (core) {
      return core->id;
   } else {
      return InvalidCoreId;
   }
}

} // namespace this_core

} // namespace cpu

#include "latte/latte_irq.h"
#include <atomic>
#include <common/decaf_assert.h>
#include <fmt/format.h>

namespace latte
{

static std::atomic_uint32_t DSP_CONTROL_STATUS { 0u };

static std::atomic_uint32_t PI_INTSR_GLOBAL { 0u };
static std::atomic_uint32_t PI_INTSR_CPU0 { 0u };
static std::atomic_uint32_t PI_INTSR_CPU1 { 0u };
static std::atomic_uint32_t PI_INTSR_CPU2 { 0u };

static std::atomic_uint32_t PI_INTMR_GLOBAL { 0u };
static std::atomic_uint32_t PI_INTMR_CPU0 { 0u };
static std::atomic_uint32_t PI_INTMR_CPU1 { 0u };
static std::atomic_uint32_t PI_INTMR_CPU2 { 0u };

static std::atomic_uint32_t LT_INTSR_AHBALL_ARM { AhbAll { 0u } };
static std::atomic_uint32_t LT_INTSR_AHBALL_PPC0 { AhbAll { 0u } };
static std::atomic_uint32_t LT_INTSR_AHBALL_PPC1 { AhbAll { 0u } };
static std::atomic_uint32_t LT_INTSR_AHBALL_PPC2 { AhbAll { 0u } };

static std::atomic_uint32_t LT_INTMR_AHBALL_ARM { AhbAll { 0u } };
static std::atomic_uint32_t LT_INTMR_AHBALL_PPC0 { AhbAll { 0u } };
static std::atomic_uint32_t LT_INTMR_AHBALL_PPC1 { AhbAll { 0u } };
static std::atomic_uint32_t LT_INTMR_AHBALL_PPC2 { AhbAll { 0u } };

static std::atomic_uint32_t LT_INTSR_AHBLT_ARM { AhbLt { 0u } };
static std::atomic_uint32_t LT_INTSR_AHBLT_PPC0 { AhbLt { 0u } };
static std::atomic_uint32_t LT_INTSR_AHBLT_PPC1 { AhbLt { 0u } };
static std::atomic_uint32_t LT_INTSR_AHBLT_PPC2 { AhbLt { 0u } };

static std::atomic_uint32_t LT_INTMR_AHBLT_ARM { AhbLt { 0u } };
static std::atomic_uint32_t LT_INTMR_AHBLT_PPC0 { AhbLt { 0u } };
static std::atomic_uint32_t LT_INTMR_AHBLT_PPC1 { AhbLt { 0u } };
static std::atomic_uint32_t LT_INTMR_AHBLT_PPC2 { AhbLt { 0u } };

void
setIrqRegister(IrqRegisters id,
               uint32_t value)
{
   switch (id) {
   case IrqRegisters::LT_INTSR_AHBALL_PPC0:
      LT_INTSR_AHBALL_PPC0 |= value;
      break;
   case IrqRegisters::LT_INTSR_AHBLT_PPC0:
      LT_INTSR_AHBLT_PPC0 |= value;
      break;
   case IrqRegisters::LT_INTMR_AHBALL_PPC0:
      LT_INTMR_AHBALL_PPC0 |= value;
      break;
   case IrqRegisters::LT_INTMR_AHBLT_PPC0:
      LT_INTMR_AHBLT_PPC0 |= value;
      break;
   case IrqRegisters::LT_INTSR_AHBALL_PPC1:
      LT_INTSR_AHBALL_PPC1 |= value;
      break;
   case IrqRegisters::LT_INTSR_AHBLT_PPC1:
      LT_INTSR_AHBLT_PPC1 |= value;
      break;
   case IrqRegisters::LT_INTMR_AHBALL_PPC1:
      LT_INTMR_AHBALL_PPC1 |= value;
      break;
   case IrqRegisters::LT_INTMR_AHBLT_PPC1:
      LT_INTMR_AHBLT_PPC1 |= value;
      break;
   case IrqRegisters::LT_INTSR_AHBALL_PPC2:
      LT_INTSR_AHBALL_PPC2 |= value;
      break;
   case IrqRegisters::LT_INTSR_AHBLT_PPC2:
      LT_INTSR_AHBLT_PPC2 |= value;
      break;
   case IrqRegisters::LT_INTMR_AHBALL_PPC2:
      LT_INTMR_AHBALL_PPC2 |= value;
      break;
   case IrqRegisters::LT_INTMR_AHBLT_PPC2:
      LT_INTMR_AHBLT_PPC2 |= value;
      break;
   case IrqRegisters::LT_INTSR_AHBALL_ARM:
      LT_INTSR_AHBALL_ARM |= value;
      break;
   case IrqRegisters::LT_INTSR_AHBLT_ARM:
      LT_INTSR_AHBLT_ARM |= value;
      break;
   case IrqRegisters::LT_INTMR_AHBALL_ARM:
      LT_INTMR_AHBALL_ARM |= value;
      break;
   case IrqRegisters::LT_INTMR_AHBLT_ARM:
      LT_INTMR_AHBLT_ARM |= value;
      break;
   default:
      decaf_abort(fmt::format("Unknown IRQ Register {}", id));
   }
}


void
setAhbAllRegister(uint32_t core,
                  AhbAll value)
{
   decaf_check(core < 3);

   switch (core) {
   case 0:
      LT_INTSR_AHBALL_PPC0 |= value;
      break;
   case 1:
      LT_INTSR_AHBALL_PPC1 |= value;
      break;
   case 2:
      LT_INTSR_AHBALL_PPC2 |= value;
      break;
   }
}


void
setAhbLtRegister(uint32_t core,
                 AhbLt value)
{
   decaf_check(core < 3);

   switch (core) {
   case 0:
      LT_INTSR_AHBLT_PPC0 |= value;
      break;
   case 1:
      LT_INTSR_AHBLT_PPC1 |= value;
      break;
   case 2:
      LT_INTSR_AHBLT_PPC2 |= value;
      break;
   }
}

} // namespace latte

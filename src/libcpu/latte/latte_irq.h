#pragma once
#include <common/enum_start.h>

// TODO: One day move to liblatte?

namespace latte
{

ENUM_BEG(IrqRegisters, uint32_t)
   ENUM_VALUE(PI_INTSR_GLOBAL,            0x0C000000)
   ENUM_VALUE(PI_INTMR_GLOBAL,            0x0C000004)

   ENUM_VALUE(PI_INTSR_CPU0,              0x0C000078)
   ENUM_VALUE(PI_INTMR_CPU0,              0x0C00007C)
   ENUM_VALUE(PI_INTSR_CPU1,              0x0C000080)
   ENUM_VALUE(PI_INTMR_CPU1,              0x0C000084)
   ENUM_VALUE(PI_INTSR_CPU2,              0x0C000088)
   ENUM_VALUE(PI_INTMR_CPU2,              0x0C00008C)

   ENUM_VALUE(DSP_CONTROL_STATUS,         0x0C28000A)

   ENUM_VALUE(LT_INTSR_PPC_COMPAT,        0x0D800030)
   ENUM_VALUE(LT_INTMR_PPC_COMPAT,        0x0D800034)
   ENUM_VALUE(LT_INTSR_ARM_COMPAT,        0x0D800038)
   ENUM_VALUE(LT_INTMR_ARM_COMPAT,        0x0D80003C)

   ENUM_VALUE(LT_INTSR_AHBALL_PPC0,       0x0D800440)
   ENUM_VALUE(LT_INTSR_AHBLT_PPC0,        0x0D800444)
   ENUM_VALUE(LT_INTMR_AHBALL_PPC0,       0x0D800448)
   ENUM_VALUE(LT_INTMR_AHBLT_PPC0,        0x0D80044C)

   ENUM_VALUE(LT_INTSR_AHBALL_PPC1,       0x0D800450)
   ENUM_VALUE(LT_INTSR_AHBLT_PPC1,        0x0D800454)
   ENUM_VALUE(LT_INTMR_AHBALL_PPC1,       0x0D800458)
   ENUM_VALUE(LT_INTMR_AHBLT_PPC1,        0x0D80045C)

   ENUM_VALUE(LT_INTSR_AHBALL_PPC2,       0x0D800460)
   ENUM_VALUE(LT_INTSR_AHBLT_PPC2,        0x0D800464)
   ENUM_VALUE(LT_INTMR_AHBALL_PPC2,       0x0D800468)
   ENUM_VALUE(LT_INTMR_AHBLT_PPC2,        0x0D80046C)

   ENUM_VALUE(LT_INTSR_AHBALL_ARM,        0x0D800470)
   ENUM_VALUE(LT_INTSR_AHBLT_ARM,         0x0D800474)
   ENUM_VALUE(LT_INTMR_AHBALL_ARM,        0x0D800478)
   ENUM_VALUE(LT_INTMR_AHBLT_ARM,         0x0D80047C)
ENUM_END(IrqRegisters)

FLAGS_BEG(PiAll, uint32_t)
   FLAGS_VALUE(Error,                     1u << 0)
   FLAGS_VALUE(Dsp,                       1u << 6)
FLAGS_END(PiAll)

FLAGS_BEG(PiLatte, uint32_t)
   FLAGS_VALUE(Gpu7,                      1u << 23)
   FLAGS_VALUE(Ahb,                       1u << 24)
FLAGS_END(PiLatte)

FLAGS_BEG(DspControlStatus, uint32_t)
   FLAGS_VALUE(AiInterruptStatus,         1u << 3)
   FLAGS_VALUE(AiInterruptMask,           1u << 4)
   FLAGS_VALUE(AccInterruptStatus,        1u << 5)
   FLAGS_VALUE(AccInterruptMask,          1u << 6)
   FLAGS_VALUE(DmaInterruptStatus,        1u << 7)
   FLAGS_VALUE(DmaInterruptMask,          1u << 8)
   FLAGS_VALUE(Ai2InterruptStatus,        1u << 12)
   FLAGS_VALUE(Ai2InterruptMask,          1u << 13)
FLAGS_END(DspControlStatus)

FLAGS_BEG(AhbAll, uint32_t)
   FLAGS_VALUE(Timer,                     1u << 0)
   FLAGS_VALUE(NandInterface,             1u << 1)
   FLAGS_VALUE(AesEngine,                 1u << 2)
   FLAGS_VALUE(Sha1Engine,                1u << 3)
   FLAGS_VALUE(UsbEhci,                   1u << 4)
   FLAGS_VALUE(UsbOhci0,                  1u << 5)
   FLAGS_VALUE(UsbOhci1,                  1u << 6)
   FLAGS_VALUE(SdHostController,          1u << 7)
   FLAGS_VALUE(Wireless80211,             1u << 8)
   FLAGS_VALUE(LatteGpioEspresso,         1u << 10)
   FLAGS_VALUE(LatteGpioStarbuck,         1u << 11)
   FLAGS_VALUE(SysProt,                   1u << 12)
   FLAGS_VALUE(PowerButton,               1u << 17)
   FLAGS_VALUE(DriveInterface,            1u << 18)
   FLAGS_VALUE(ExiRtc,                    1u << 20)
   FLAGS_VALUE(Sata,                      1u << 28)
   FLAGS_VALUE(IpcEspressoCompat,         1u << 30)
   FLAGS_VALUE(IpcStarbuckCompat,         1u << 31)
FLAGS_END(AhbAll)

FLAGS_BEG(AhbLt, uint32_t)
   FLAGS_VALUE(SdHostController,          1u << 0)
   FLAGS_VALUE(Unknown1,                  1u << 1)
   FLAGS_VALUE(Unknown2,                  1u << 2)
   FLAGS_VALUE(Unknown3,                  1u << 3)
   FLAGS_VALUE(Drh,                       1u << 4)
   FLAGS_VALUE(Unknown5,                  1u << 5)
   FLAGS_VALUE(Unknown6,                  1u << 6)
   FLAGS_VALUE(Unknown7,                  1u << 7)
   FLAGS_VALUE(AesEngine,                 1u << 8)
   FLAGS_VALUE(Sha1Engine,                1u << 9)
   FLAGS_VALUE(Unknown10,                 1u << 10)
   FLAGS_VALUE(Unknown11,                 1u << 11)
   FLAGS_VALUE(Unknown12,                 1u << 12)
   FLAGS_VALUE(I2CEspresso,               1u << 13)
   FLAGS_VALUE(I2CStarbuck,               1u << 14)
   FLAGS_VALUE(IpcEspressoCore2,          1u << 26)
   FLAGS_VALUE(IpcStarbuckCore2,          1u << 27)
   FLAGS_VALUE(IpcEspressoCore1,          1u << 28)
   FLAGS_VALUE(IpcStarbuckCore1,          1u << 29)
   FLAGS_VALUE(IpcEspressoCore0,          1u << 30)
   FLAGS_VALUE(IpcStarbuckCore0,          1u << 31)
FLAGS_END(AhbLt)

void
setIrqRegister(IrqRegisters id,
               uint32_t value);

void
setAhbAllRegister(uint32_t core,
                  AhbAll value);

void
setAhbLtRegister(uint32_t core,
                 AhbLt value);

} // namespace latte

#include <common/enum_end.h>

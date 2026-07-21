#ifndef HW_ARM_S32K389_H
#define HW_ARM_S32K389_H

#include "hw/arm/armv7m.h"
#include "hw/core/boards.h"
#include "hw/core/sysbus.h"
#include "qemu/osdep.h"
#include "qemu/units.h"
#include "qom/object.h"
#include "net/can_emu.h"

#define TYPE_S32K389 MACHINE_TYPE_NAME("s32k389")
OBJECT_DECLARE_SIMPLE_TYPE(S32K389State, S32K389)

// System frequency definitions
#define S32K3_SYSCLK_FREQ           (320 * 1000 * 1000)  // 320MHz

// Memory definitions
#define S32K389_TCM_COUNT           4

/*
 * The S32K389 has one local ITCM/DTCM aperture per Cortex-M7 core.  Those
 * local apertures overlap architecturally: every core sees its own ITCM at
 * 0x00000000 and its own DTCM at 0x20000000.  In this single-core QEMU board
 * we map core 0 at the local addresses and expose all implemented TCMs through
 * their unique backdoor addresses.
 */
#define INT_ITCM_BASE               0x00000000
#define INT_ITCM_SIZE               (32 * KiB)
#define INT_DTCM_BASE               0x20000000
#define INT_DTCM_SIZE               (64 * KiB)

#define INT_ITCM0_BACKDOOR_BASE     0x11000000
#define INT_ITCM1_BACKDOOR_BASE     0x11400000
#define INT_ITCM2_BACKDOOR_BASE     0x11800000
#define INT_ITCM3_BACKDOOR_BASE     0x11C00000

#define INT_DTCM0_BACKDOOR_BASE     0x21000000
#define INT_DTCM1_BACKDOOR_BASE     0x21400000
#define INT_DTCM2_BACKDOOR_BASE     0x21800000
#define INT_DTCM3_BACKDOOR_BASE     0x21C00000

// Flash
#define FLASH_SIZE                  0x00822000
#define INT_CODE_FLASH0_BASE        0x00400000
#define INT_CODE_FLASH0_SIZE        0x00200000  // 2 MB


/* The FlexCAN example firmware places its reset vector table at 0x00402000. */
#define INT_CODE_FLASH0_CORE0_VTOR  0x00402000
#define INT_CODE_FLASH1_BASE        0x00600000
#define INT_CODE_FLASH1_SIZE        0x00200000  // 2 MB
#define INT_CODE_FLASH2_BASE        0x00800000
#define INT_CODE_FLASH2_SIZE        0x00200000  // 2 MB
#define INT_CODE_FLASH3_BASE        0x00A00000
#define INT_CODE_FLASH3_SIZE        0x00200000  // 2 MB
#define INT_DATA_FLASH_BASE         0x10000000
#define INT_DATA_FLASH_SIZE         0x00020000  // 128 KB
#define INT_UTEST_NVM_FLASH_BASE    0x1B000000
#define INT_UTEST_NVM_FLASH_SIZE    0x00002000  // 8 KB

// SRAM
#define SRAM_SIZE                   0xC0000
#define INT_SRAM_STANDBY_BASE       0x20400000  // SRAM_standby, SPLIT FROM SRAM0
#define INT_SRAM_STANDBY_SIZE       0x10000     // 64KB
#define INT_SRAM_0_BASE             0x20410000
#define INT_SRAM_0_SIZE             0x30000     // 256KB-64KB = 192KB
#define INT_SRAM_1_BASE             0x20440000
#define INT_SRAM_1_SIZE             0x40000     // 256KB
#define INT_SRAM_2_BASE             0x20480000
#define INT_SRAM_2_SIZE             0x40000     // 256KB

// Peripheral definitions
#define S32K3_PERIPH_BASE           0x40000000

// LPUART
#define S32K3_UART_BASE             0x40328000
#define S32K3_LPUART1_BASE          0x4032C000
#define S32K3_LPUART2_BASE          0x40330000
#define S32K3_LPUART3_BASE          0x40334000
#define S32K3_LPUART4_BASE          0x40338000
#define S32K3_LPUART5_BASE          0x4033C000
#define S32K3_LPUART6_BASE          0x40340000
#define S32K3_LPUART7_BASE          0x40344000
#define S32K3_LPUART8_BASE          0x4048C000
#define S32K3_LPUART9_BASE          0x40490000
#define S32K3_LPUART10_BASE         0x40494000
#define S32K3_LPUART11_BASE         0x40498000
#define S32K3_LPUART12_BASE         0x4049C000
#define S32K3_LPUART13_BASE         0x404A0000
#define S32K3_LPUART14_BASE         0x404A4000
#define S32K3_LPUART15_BASE         0x404A8000

#define S32K3_CONSOLE_LPUART_BASE S32K3_LPUART3_BASE
#define S32K3_CONSOLE_LPUART_IRQ  144

// FlexIO instance used by the UART loopback example
#define S32K3_FLEXIO_BASE          0x40324000
#define S32K3_FLEXIO_IRQ           139

// LPSPI
#define S32K389_NUM_LPSPI 6
#define S32K3_LPSPI0_BASE (S32K3_PERIPH_BASE + 0x358000)  // 0x40358000
#define S32K3_LPSPI1_BASE (S32K3_PERIPH_BASE + 0x35C000)  // 0x4035C000
#define S32K3_LPSPI2_BASE (S32K3_PERIPH_BASE + 0x360000)  // 0x40360000
#define S32K3_LPSPI3_BASE (S32K3_PERIPH_BASE + 0x364000)  // 0x40364000
#define S32K3_LPSPI4_BASE (S32K3_PERIPH_BASE + 0x4BC000)  // 0x404BC000
#define S32K3_LPSPI5_BASE (S32K3_PERIPH_BASE + 0x4C0000)  // 0x404C0000
// LPSPI Interrupts
#define S32K3_LPSPI0_IRQ 69
#define S32K3_LPSPI1_IRQ 70
#define S32K3_LPSPI2_IRQ 71
#define S32K3_LPSPI3_IRQ 72
#define S32K3_LPSPI4_IRQ 73
#define S32K3_LPSPI5_IRQ 74

// FlexCAN
#define S32K389_CAN_COUNT   6
#define S32K3_FLEXCAN0_BASE      (S32K3_PERIPH_BASE + 0x304000) /* 0x40304000 */
#define S32K3_FLEXCAN1_BASE      (S32K3_PERIPH_BASE + 0x308000) /* 0x40308000 */
#define S32K3_FLEXCAN2_BASE      (S32K3_PERIPH_BASE + 0x30C000) /* 0x4030C000 */
#define S32K3_FLEXCAN3_BASE      (S32K3_PERIPH_BASE + 0x310000) /* 0x40310000 */
#define S32K3_FLEXCAN4_BASE      (S32K3_PERIPH_BASE + 0x314000) /* 0x40314000 */
#define S32K3_FLEXCAN5_BASE      (S32K3_PERIPH_BASE + 0x318000) /* 0x40318000 */
#define S32K3_FLEXCAN0_MB_IRQ    110
#define S32K3_FLEXCAN1_MB_IRQ    114
#define S32K3_FLEXCAN2_MB_IRQ    117
#define S32K3_FLEXCAN3_MB_IRQ    120
#define S32K3_FLEXCAN4_MB_IRQ    122
#define S32K3_FLEXCAN5_MB_IRQ    124

// Boot state
#define S32K3_MC_ME_BASE        0x402D0000
#define S32K3_MC_ME_SIZE        0x20000
#define S32K3_BOOT_STATUS_BASE   0x402DC000
#define S32K3_BOOT_STATUS_SIZE   0x1000
#define S32K3_BOOT_STATUS_GS     0x310
#define S32K3_BOOT_STATUS_PCS    0x510
#define S32K3_BOOT_STATUS_CTL_STAT 0x504
#define S32K3_BOOT_STATUS_CLOCK_READY BIT(24)

typedef struct S32K389State {
  MachineState parent_obj;

  ARMv7MState armv7m;

  Clock* sysclk;

  DeviceState* uart;
  DeviceState* flexio;
  DeviceState* lpspi[S32K389_NUM_LPSPI];
  DeviceState *flexcan[S32K389_CAN_COUNT];
  CanBusState *canbus[S32K389_CAN_COUNT];

  MemoryRegion itcm[S32K389_TCM_COUNT];
  MemoryRegion dtcm[S32K389_TCM_COUNT];
  MemoryRegion itcm_backdoor[S32K389_TCM_COUNT];
  MemoryRegion dtcm_backdoor[S32K389_TCM_COUNT];
  MemoryRegion C0flash;
  MemoryRegion C1flash;
  MemoryRegion C2flash;
  MemoryRegion C3flash;
  MemoryRegion Dflash;
  MemoryRegion UNVMflash;

  MemoryRegion sram_standby;
  MemoryRegion sram0;
  MemoryRegion sram1;
  MemoryRegion sram2;

  MemoryRegion boot_status;
  MemoryRegion mc_me;
  uint32_t mc_me_regs[S32K3_MC_ME_SIZE / sizeof(uint32_t)];
} S32K389State;

#endif

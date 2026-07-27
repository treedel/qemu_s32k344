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
#define INT_ITCM_BASE               0x00000000  // Instruction Tightly Coupled Memory
#define INT_ITCM_SIZE               (64 * KiB)  // 64KB
 
#define INT_DTCM_BASE               0x20000000  // Data Tightly Coupled Memory
#define INT_DTCM_SIZE               (124 * KiB) // 124KB
#define INT_DTCM_STACK_BASE         0x2001F000  // Stack area in DTCM
#define INT_DTCM_STACK_SIZE         (4 * KiB)   // 4KB
 
// Flash
// S32K389 has TWO program-flash controllers (PFC0 + PFC1), unlike S32K388's
// single controller - verified against S32K3xx_memory_map.xlsx. The 8 blocks
// below are contiguous (0x00400000-0x00FFFFFF) and sum to 12MB, matching the
// NXP datasheet block diagram ("Figure 16. S32K389: ASIL D 12MB MCU").
#define FLASH_SIZE                  0x00C00000  // 12 MB total, contiguous
#define INT_CODE_FLASH0_BASE        0x00400000  // PFC1 Block 0
#define INT_CODE_FLASH0_SIZE        0x00100000  // 1 MB
/* The FlexCAN example firmware places its reset vector table at 0x00402000. */
#define INT_CODE_FLASH0_CORE0_VTOR  0x00402000
#define INT_CODE_FLASH1_BASE        0x00500000  // PFC1 Block 1
#define INT_CODE_FLASH1_SIZE        0x00100000  // 1 MB
#define INT_CODE_FLASH2_BASE        0x00600000  // PFC0 Block 0
#define INT_CODE_FLASH2_SIZE        0x00200000  // 2 MB
#define INT_CODE_FLASH3_BASE        0x00800000  // PFC0 Block 1
#define INT_CODE_FLASH3_SIZE        0x00200000  // 2 MB
#define INT_CODE_FLASH4_BASE        0x00A00000  // PFC1 Block 2
#define INT_CODE_FLASH4_SIZE        0x00100000  // 1 MB
#define INT_CODE_FLASH5_BASE        0x00B00000  // PFC1 Block 3
#define INT_CODE_FLASH5_SIZE        0x00100000  // 1 MB
#define INT_CODE_FLASH6_BASE        0x00C00000  // PFC0 Block 2
#define INT_CODE_FLASH6_SIZE        0x00200000  // 2 MB
#define INT_CODE_FLASH7_BASE        0x00E00000  // PFC0 Block 3
#define INT_CODE_FLASH7_SIZE        0x00200000  // 2 MB
#define INT_DATA_FLASH_BASE         0x10000000
#define INT_DATA_FLASH_SIZE         0x00040000  // 256 KB (S32K389; S32K388 is 128KB)
#define INT_UTEST_NVM_FLASH_BASE    0x1B000000
#define INT_UTEST_NVM_FLASH_SIZE    0x00002000  // 8 KB (same on both chips)
 
// SRAM
// S32K389 has 4 SRAM banks (512+512+512+384 = 1920KB) vs S32K388's 3 banks
// (256+256+256 = 768KB) - verified against S32K3xx_memory_map.xlsx. The 64KB
// "standby" carve-out at the front of SRAM0 is preserved from the original
// model (a low-power retention RAM feature, independent of total SRAM grade,
// not broken out as its own row in the memory-map sheet).
#define SRAM_SIZE                   0x1E0000    // 1920KB (512+512+512+384)
#define INT_SRAM_STANDBY_BASE       0x20400000  // SRAM_standby, split from SRAM0
#define INT_SRAM_STANDBY_SIZE       0x10000     // 64KB
#define INT_SRAM_0_BASE             0x20410000
#define INT_SRAM_0_SIZE             0x70000     // 512KB - 64KB = 448KB
#define INT_SRAM_1_BASE             0x20480000
#define INT_SRAM_1_SIZE             0x80000     // 512KB
#define INT_SRAM_2_BASE             0x20500000
#define INT_SRAM_2_SIZE             0x80000     // 512KB
#define INT_SRAM_3_BASE             0x20580000  // S32K389-only bank (S32K388 has no SRAM3)
#define INT_SRAM_3_SIZE             0x60000     // 384KB
 
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
#define S32K3_CONSOLE_LPUART_IRQ  144  // verified: LPUART 3 = IRQ 144 in interrupt map
 
// FlexIO instance used by the UART loopback example
#define S32K3_FLEXIO_BASE          0x40324000
#define S32K3_FLEXIO_IRQ           139  // verified: "Flexible IO" = IRQ 139 in interrupt map
 
// LPSPI
#define S32K389_NUM_LPSPI 6
#define S32K3_LPSPI0_BASE (S32K3_PERIPH_BASE + 0x358000)  // 0x40358000
#define S32K3_LPSPI1_BASE (S32K3_PERIPH_BASE + 0x35C000)  // 0x4035C000
#define S32K3_LPSPI2_BASE (S32K3_PERIPH_BASE + 0x360000)  // 0x40360000
#define S32K3_LPSPI3_BASE (S32K3_PERIPH_BASE + 0x364000)  // 0x40364000
#define S32K3_LPSPI4_BASE (S32K3_PERIPH_BASE + 0x4BC000)  // 0x404BC000
#define S32K3_LPSPI5_BASE (S32K3_PERIPH_BASE + 0x4C0000)  // 0x404C0000
// LPSPI Interrupts
// Verified against S32K3xx_interrupt_map.xlsx. NOTE: the previous values
// here (69-74) were wrong - those IRQs actually belong to eMIOS1 per the
// interrupt map, not LPSPI. Corrected below.
#define S32K3_LPSPI0_IRQ 165
#define S32K3_LPSPI1_IRQ 166
#define S32K3_LPSPI2_IRQ 167
#define S32K3_LPSPI3_IRQ 168
#define S32K3_LPSPI4_IRQ 169
#define S32K3_LPSPI5_IRQ 170
 
// FlexCAN
// Per S32K3xx_memory_map.xlsx (Peripherals sheet): S32K389 populates FlexCAN
// 0-11 (12 instances total), vs. S32K388 which only populates FlexCAN 0-7.
// Addresses below are taken directly from that sheet's Start/End address
// columns for the S32K389 column.
#define S32K389_CAN_COUNT   12
#define S32K3_FLEXCAN0_BASE      (S32K3_PERIPH_BASE + 0x304000) /* 0x40304000 */
#define S32K3_FLEXCAN1_BASE      (S32K3_PERIPH_BASE + 0x308000) /* 0x40308000 */
#define S32K3_FLEXCAN2_BASE      (S32K3_PERIPH_BASE + 0x30C000) /* 0x4030C000 */
#define S32K3_FLEXCAN3_BASE      (S32K3_PERIPH_BASE + 0x310000) /* 0x40310000 */
#define S32K3_FLEXCAN4_BASE      (S32K3_PERIPH_BASE + 0x314000) /* 0x40314000 */
#define S32K3_FLEXCAN5_BASE      (S32K3_PERIPH_BASE + 0x318000) /* 0x40318000 */
#define S32K3_FLEXCAN6_BASE      (S32K3_PERIPH_BASE + 0x31C000) /* 0x4031C000 */
#define S32K3_FLEXCAN7_BASE      (S32K3_PERIPH_BASE + 0x320000) /* 0x40320000 */
#define S32K3_FLEXCAN8_BASE      0x40570000  /* S32K389-only */
#define S32K3_FLEXCAN9_BASE      0x40574000  /* S32K389-only */
#define S32K3_FLEXCAN10_BASE     0x40578000  /* S32K389-only */
#define S32K3_FLEXCAN11_BASE     0x4057C000  /* S32K389-only */
 
#define S32K3_FLEXCAN0_MB_IRQ    110
#define S32K3_FLEXCAN1_MB_IRQ    114
#define S32K3_FLEXCAN2_MB_IRQ    117
#define S32K3_FLEXCAN3_MB_IRQ    120
#define S32K3_FLEXCAN4_MB_IRQ    122
#define S32K3_FLEXCAN5_MB_IRQ    124
/*
* Verified against S32K3xx_interrupt_map.xlsx ("Interrupts" sheet, S32K389
* column). Each FlexCAN instance has multiple IRQ lines (Bus Off, MB line 0,
* MB line 32, MB line 64...); to match the convention already used for
* instances 0-5 above, these use the "Message Buffer Interrupt line 0" IRQ
* for each instance (i.e. the "_1" row for FlexCANn in that sheet).
*/
#define S32K3_FLEXCAN6_MB_IRQ    126
#define S32K3_FLEXCAN7_MB_IRQ    128
#define S32K3_FLEXCAN8_MB_IRQ    60
#define S32K3_FLEXCAN9_MB_IRQ    75
#define S32K3_FLEXCAN10_MB_IRQ   104
#define S32K3_FLEXCAN11_MB_IRQ   164
 
// Boot / clock-gen state
//
// NOTE ON NAMING (verified against manual chapter 46 "Mode Entry Module
// (MC_ME)" and S32K3xx_memory_map.xlsx):
//   - The real MC_ME peripheral's base address is 402D_C000h (manual
//     section 46.7.1 states this explicitly). That is what used to be
//     called S32K3_BOOT_STATUS_BASE in this file - it was the correct
//     address all along, just mislabeled.
//   - The address range 0x402D0000-0x402DBFFF (48 KB) that used to be
//     labeled S32K3_MC_ME_BASE/SIZE does NOT correspond to real MC_ME at
//     all. Per the memory map spreadsheet, that range actually belongs to
//     FIRC (0x402D0000), FXOSC (0x402D4000), and MC_CGM (0x402D8000) - three
//     separate clock-generation peripherals. It's kept here as a generic
//     read/write register stub (not a real model of those three modules)
//     so firmware probing those addresses doesn't fault.
#define S32K3_CLKGEN_STUB_BASE   0x402D0000  // covers FIRC/FXOSC/MC_CGM ranges
#define S32K3_CLKGEN_STUB_SIZE   0xC000      // 48KB, ends exactly where real MC_ME starts
#define S32K3_MC_ME_BASE         0x402DC000  // real MC_ME base per manual 46.7.1
#define S32K3_MC_ME_SIZE         0x4000      // 16KB per memory map spreadsheet
/*
* Offsets below are real MC_ME register offsets (see manual 46.7.1 "MC_ME
* memory map"), renamed from their previous generic "boot status" names.
* The values returned for them are still simplified stand-ins used to get
* guest firmware clock-ready checks to pass at boot - NOT the datasheet
* reset values - kept behaviorally unchanged from the original model so as
* not to alter existing boot behavior.
*/
#define S32K3_MC_ME_PRTN1_COFB0_STAT   0x310  // real reset value: 0x5E3F_0007
#define S32K3_MC_ME_PRTN2_PUPD         0x504  // real reset value: 0x0000_0000
#define S32K3_MC_ME_PRTN2_COFB0_STAT   0x510  // real reset value: 0x0600_000F
#define S32K3_MC_ME_CLOCK_READY_BIT    BIT(24) // simplified stand-in, not a real MC_ME bit
 
typedef struct S32K389State {
  MachineState parent_obj;
 
  ARMv7MState armv7m;
 
  Clock* sysclk;
 
  DeviceState* uart;
  DeviceState* flexio;
  DeviceState* lpspi[S32K389_NUM_LPSPI];
  DeviceState *flexcan[S32K389_CAN_COUNT];
  CanBusState *canbus[S32K389_CAN_COUNT];
 
  MemoryRegion itcm;
  MemoryRegion dtcm;
  MemoryRegion dtcm_stack;
  MemoryRegion flash0;
  MemoryRegion flash1;
  MemoryRegion flash2;
  MemoryRegion flash3;
  MemoryRegion flash4;
  MemoryRegion flash5;
  MemoryRegion flash6;
  MemoryRegion flash7;
  MemoryRegion Dflash;
  MemoryRegion UNVMflash;
 
  MemoryRegion sram_standby;
  MemoryRegion sram0;
  MemoryRegion sram1;
  MemoryRegion sram2;
  MemoryRegion sram3;
 
  MemoryRegion mc_me;         // real MC_ME peripheral (0x402DC000, switch-based ops)
  MemoryRegion clkgen_stub;   // generic stub for FIRC/FXOSC/MC_CGM range (0x402D0000)
  uint32_t clkgen_regs[S32K3_CLKGEN_STUB_SIZE / sizeof(uint32_t)];
} S32K389State;
 
#endif
 
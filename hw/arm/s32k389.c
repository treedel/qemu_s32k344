/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "system/memory.h"
#include "system/address-spaces.h"
#include "system/system.h"
#include "hw/core/qdev.h"
#include "hw/core/sysbus.h"
#include "hw/arm/s32k389.h"
#include "hw/char/s32k3_uart.h"
#include "hw/net/s32k3_flexcan.h"
#include "hw/char/s32k3_flexio_uart.h"
#include "hw/arm/boot.h"
#include "hw/arm/machines-qom.h"
#include "hw/core/qdev-properties.h"
#include "hw/core/qdev-clock.h"
#include "hw/core/clock.h"
#include "hw/misc/unimp.h"
#include "qemu/units.h"
#include "semihosting/semihost.h"
#include "migration/vmstate.h"
#include "hw/core/irq.h"
#include <stdio.h>
#include <time.h>
 
 
/*
* Real MC_ME peripheral (base 0x402DC000, verified against manual section
* 46.7.1 "MC_ME memory map"). Only the specific offsets firmware checks at
* boot are handled; values are simplified stand-ins (see S32K3_MC_ME_*
* comments in the header for the real register names/reset values).
*/
static uint64_t s32k389_mc_me_read(void *opaque, hwaddr offset, unsigned size) {
    switch (offset) {
    case S32K3_MC_ME_PRTN1_COFB0_STAT:
        return S32K3_MC_ME_CLOCK_READY_BIT;
    case S32K3_MC_ME_PRTN2_PUPD:
        return 0;
    case S32K3_MC_ME_PRTN2_COFB0_STAT:
        return UINT32_MAX;
    default:
        return UINT32_MAX;
    }
}
 
static void s32k389_mc_me_write(void *opaque, hwaddr offset, uint64_t value, unsigned size) {
    qemu_log_mask(LOG_UNIMP,
                  "s32k389.mc_me: unimplemented write "
                  "(size %u, offset 0x%" HWADDR_PRIx ", value 0x%" PRIx64 ")\n",
                  size, offset, value);
}
 
static const MemoryRegionOps s32k389_mc_me_ops = {
    .read = s32k389_mc_me_read,
    .write = s32k389_mc_me_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl.min_access_size = 1,
    .impl.max_access_size = 4,
    .valid.min_access_size = 1,
    .valid.max_access_size = 4,
};
 
/*
* Generic catch-all stub covering the FIRC/FXOSC/MC_CGM address range
* (0x402D0000-0x402DBFFF). NOT a functional model of those clock modules -
* just a plain read/write register array so firmware probing them doesn't
* fault. Previously this range was (incorrectly) labeled "mc_me".
*/
static bool s32k389_clkgen_is_cofb_status(hwaddr offset)
{
    if (offset >= 0x110 && offset < 0x120) {
        return true;
    }
 
    return offset >= 0x10000 && ((offset & 0xf) == 0x4);
}
 
static uint64_t s32k389_clkgen_read(void *opaque, hwaddr offset, unsigned size)
{
    S32K389State *s = opaque;
 
    if (size != 4 || offset + size > S32K3_CLKGEN_STUB_SIZE) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "s32k389.clkgen_stub: invalid read size %u "
                      "@0x%" HWADDR_PRIx "\n", size, offset);
        return 0;
    }
 
    if (s32k389_clkgen_is_cofb_status(offset)) {
        return UINT32_MAX;
    }
 
    return s->clkgen_regs[offset >> 2];
}
 
static void s32k389_clkgen_write(void *opaque, hwaddr offset,
                                uint64_t value, unsigned size)
{
    S32K389State *s = opaque;
 
    if (size != 4 || offset + size > S32K3_CLKGEN_STUB_SIZE) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "s32k389.clkgen_stub: invalid write size %u "
                      "@0x%" HWADDR_PRIx ", value 0x%" PRIx64 "\n",
                      size, offset, value);
        return;
    }
 
    s->clkgen_regs[offset >> 2] = value;
}
 
static const MemoryRegionOps s32k389_clkgen_ops = {
    .read = s32k389_clkgen_read,
    .write = s32k389_clkgen_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl.min_access_size = 4,
    .impl.max_access_size = 4,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
};
 
static void s32k389_init_flexcan(S32K389State *s, ARMv7MState *armv7m) {
    /*
     * Addresses verified against S32K3xx_memory_map.xlsx (Peripherals sheet,
     * S32K389 column). S32K389 populates all 12 FlexCAN instances, unlike
     * S32K388 which only has 0-7 (and the earlier S32K388 model only wired
     * up 0-5).
     */
    static const hwaddr flexcan_bases[S32K389_CAN_COUNT] = {
        S32K3_FLEXCAN0_BASE,
        S32K3_FLEXCAN1_BASE,
        S32K3_FLEXCAN2_BASE,
        S32K3_FLEXCAN3_BASE,
        S32K3_FLEXCAN4_BASE,
        S32K3_FLEXCAN5_BASE,
        S32K3_FLEXCAN6_BASE,
        S32K3_FLEXCAN7_BASE,
        S32K3_FLEXCAN8_BASE,
        S32K3_FLEXCAN9_BASE,
        S32K3_FLEXCAN10_BASE,
        S32K3_FLEXCAN11_BASE,
    };
    /*
     * Verified against S32K3xx_interrupt_map.xlsx (Interrupts sheet, S32K389
     * column). All 12 instances now have confirmed IRQ numbers - see the
     * S32K3_FLEXCANn_MB_IRQ comments in the header for details.
     */
    static const int flexcan_mb_irq[S32K389_CAN_COUNT] = {
        S32K3_FLEXCAN0_MB_IRQ,
        S32K3_FLEXCAN1_MB_IRQ,
        S32K3_FLEXCAN2_MB_IRQ,
        S32K3_FLEXCAN3_MB_IRQ,
        S32K3_FLEXCAN4_MB_IRQ,
        S32K3_FLEXCAN5_MB_IRQ,
        S32K3_FLEXCAN6_MB_IRQ,
        S32K3_FLEXCAN7_MB_IRQ,
        S32K3_FLEXCAN8_MB_IRQ,
        S32K3_FLEXCAN9_MB_IRQ,
        S32K3_FLEXCAN10_MB_IRQ,
        S32K3_FLEXCAN11_MB_IRQ,
    };
    static const uint32_t flexcan_instance[S32K389_CAN_COUNT] = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
    };
    Error *local_err = NULL;
 
    qemu_log_mask(CPU_LOG_INT, "Initializing FlexCAN instances\n");
 
    for (int i = 0; i < S32K389_CAN_COUNT; i++) {
        DeviceState *dev = qdev_new(TYPE_S32K3X8_FLEXCAN);
        s->flexcan[i] = dev;
 
        qdev_prop_set_uint32(dev, "can-instance", flexcan_instance[i]);
        if (s->canbus[i]) {
            object_property_set_link(OBJECT(dev), "canbus", OBJECT(s->canbus[i]), &error_abort);
        }
 
        if (!sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &local_err)) {
            error_reportf_err(local_err, "Failed to realize FlexCAN instance %u: ", flexcan_instance[i]);
            return;
        }
 
        sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, flexcan_bases[i]);
 
        sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0,
                            qdev_get_gpio_in(DEVICE(armv7m), flexcan_mb_irq[i]));
    }
 
    qemu_log_mask(CPU_LOG_INT, "FlexCAN instances initialized\n");
}
 
static void s32k389_init(MachineState* machine) {
    S32K389State* s = S32K389(machine);
    Error* error_local = NULL;
    DeviceState* dev;
    
    qemu_log_mask(CPU_LOG_INT, "Initializing S32K389\n");
 
    // Get system memory
    MemoryRegion* system_memory = get_system_memory();
 
    // Initialize memory
    qemu_log_mask(CPU_LOG_INT, "Initializing memory regions\n");
    // ITCM
    memory_region_init_ram(&s->itcm, NULL, "S32K389.itcm", INT_ITCM_SIZE, &error_fatal);
    memory_region_add_subregion(system_memory, INT_ITCM_BASE, &s->itcm);
 
    // DTCM
    memory_region_init_ram(&s->dtcm, NULL, "S32K389.dtcm", INT_DTCM_SIZE, &error_fatal);
    memory_region_add_subregion(system_memory, INT_DTCM_BASE, &s->dtcm);
 
    // DTCM Stack
    memory_region_init_ram(&s->dtcm_stack, NULL, "S32K389.dtcm_stack", INT_DTCM_STACK_SIZE, &error_fatal);
    memory_region_add_subregion(system_memory, INT_DTCM_STACK_BASE, &s->dtcm_stack);
 
    // Flash - 8 blocks across two controllers (PFC0 + PFC1), 12MB total
    memory_region_init_rom(&s->flash0, NULL, "S32K389.flash0", INT_CODE_FLASH0_SIZE, &error_fatal);
    memory_region_add_subregion(system_memory, INT_CODE_FLASH0_BASE, &s->flash0);
    memory_region_init_rom(&s->flash1, NULL, "S32K389.flash1", INT_CODE_FLASH1_SIZE, &error_fatal);
    memory_region_add_subregion(system_memory, INT_CODE_FLASH1_BASE, &s->flash1);
    memory_region_init_rom(&s->flash2, NULL, "S32K389.flash2", INT_CODE_FLASH2_SIZE, &error_fatal);
    memory_region_add_subregion(system_memory, INT_CODE_FLASH2_BASE, &s->flash2);
    memory_region_init_rom(&s->flash3, NULL, "S32K389.flash3", INT_CODE_FLASH3_SIZE, &error_fatal);
    memory_region_add_subregion(system_memory, INT_CODE_FLASH3_BASE, &s->flash3);
    memory_region_init_rom(&s->flash4, NULL, "S32K389.flash4", INT_CODE_FLASH4_SIZE, &error_fatal);
    memory_region_add_subregion(system_memory, INT_CODE_FLASH4_BASE, &s->flash4);
    memory_region_init_rom(&s->flash5, NULL, "S32K389.flash5", INT_CODE_FLASH5_SIZE, &error_fatal);
    memory_region_add_subregion(system_memory, INT_CODE_FLASH5_BASE, &s->flash5);
    memory_region_init_rom(&s->flash6, NULL, "S32K389.flash6", INT_CODE_FLASH6_SIZE, &error_fatal);
    memory_region_add_subregion(system_memory, INT_CODE_FLASH6_BASE, &s->flash6);
    memory_region_init_rom(&s->flash7, NULL, "S32K389.flash7", INT_CODE_FLASH7_SIZE, &error_fatal);
    memory_region_add_subregion(system_memory, INT_CODE_FLASH7_BASE, &s->flash7);
    memory_region_init_rom(&s->Dflash, NULL, "S32K389.Dflash", INT_DATA_FLASH_SIZE, &error_fatal);
    memory_region_add_subregion(system_memory, INT_DATA_FLASH_BASE, &s->Dflash);
    memory_region_init_rom(&s->UNVMflash, NULL, "S32K389.UNVMflash", INT_UTEST_NVM_FLASH_SIZE, &error_fatal);
    memory_region_add_subregion(system_memory, INT_UTEST_NVM_FLASH_BASE, &s->UNVMflash);
 
    // SRAM
    memory_region_init_ram(&s->sram_standby, NULL, "S32K389.sram_standby", INT_SRAM_STANDBY_SIZE, &error_fatal);
    memory_region_add_subregion(system_memory, INT_SRAM_STANDBY_BASE, &s->sram_standby);
    memory_region_init_ram(&s->sram0, NULL, "S32K389.sram0", INT_SRAM_0_SIZE, &error_fatal);
    memory_region_add_subregion(system_memory, INT_SRAM_0_BASE, &s->sram0);
    memory_region_init_ram(&s->sram1, NULL, "S32K389.sram1", INT_SRAM_1_SIZE, &error_fatal);
    memory_region_add_subregion(system_memory, INT_SRAM_1_BASE, &s->sram1);
    memory_region_init_ram(&s->sram2, NULL, "S32K389.sram2", INT_SRAM_2_SIZE, &error_fatal);
    memory_region_add_subregion(system_memory, INT_SRAM_2_BASE, &s->sram2);
    memory_region_init_ram(&s->sram3, NULL, "S32K389.sram3", INT_SRAM_3_SIZE, &error_fatal);
    memory_region_add_subregion(system_memory, INT_SRAM_3_BASE, &s->sram3);
 
    qemu_log_mask(CPU_LOG_INT, "Memory regions successfully initialized\n");
 
    // Initialize system clock
    s->sysclk = clock_new(OBJECT(machine), "SYSCLK");
    clock_set_hz(s->sysclk, S32K3_SYSCLK_FREQ);
 
    // Initialize ARM core
    object_initialize_child(OBJECT(machine), "armv7m", &s->armv7m, TYPE_ARMV7M);
 
    // Configure CPU
    qdev_prop_set_string(DEVICE(&s->armv7m), "cpu-type", ARM_CPU_TYPE_NAME("cortex-m7"));
    qdev_prop_set_uint32(DEVICE(&s->armv7m), "init-svtor", INT_CODE_FLASH0_CORE0_VTOR);
    qdev_prop_set_uint32(DEVICE(&s->armv7m), "init-nsvtor", INT_CODE_FLASH0_CORE0_VTOR);
    qdev_prop_set_uint32(DEVICE(&s->armv7m), "mpu-ns-regions", 0);
    qdev_prop_set_uint32(DEVICE(&s->armv7m), "mpu-s-regions", 0);
    qdev_prop_set_uint8(DEVICE(&s->armv7m), "num-prio-bits", 4);
    qdev_prop_set_uint32(DEVICE(&s->armv7m), "num-irq", 240);
 
    // Set up system connections
    object_property_set_link(OBJECT(&s->armv7m), "memory", OBJECT(system_memory), &error_abort);
    qdev_connect_clock_in(DEVICE(&s->armv7m), "cpuclk", s->sysclk);
 
    // Implement system bus device
    sysbus_realize(SYS_BUS_DEVICE(&s->armv7m), &error_local);
 
    // Initialize UART
    qemu_log_mask(CPU_LOG_INT, "Initializing UART\n");
    dev = qdev_new(TYPE_S32E8_LPUART);
 
    // Configure UART
    qdev_prop_set_chr(dev, "chardev", serial_hd(0));
    qdev_prop_set_uint32(dev, "lpuart_id", 3);
    s->uart = dev;
 
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_local);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, S32K3_CONSOLE_LPUART_BASE);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, qdev_get_gpio_in(DEVICE(&s->armv7m), S32K3_CONSOLE_LPUART_IRQ));
 
    // Initialize the FlexIO UART channels used for board-level loopback
    dev = qdev_new(TYPE_S32K3_FLEXIO_UART);
    s->flexio = dev;
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_local);
    s32k3_flexio_uart_connect_lpuart(S32K3_FLEXIO_UART(dev), S32K3X8_LPUART(s->uart));
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, S32K3_FLEXIO_BASE);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, qdev_get_gpio_in(DEVICE(&s->armv7m), S32K3_FLEXIO_IRQ));
 
    // Initialize FlexCAN devices
    s32k389_init_flexcan(s, &s->armv7m);
 
    // Map any missing S32K3 peripheral region used by firmware
    create_unimplemented_device("s32k3x8.peripherals", S32K3_PERIPH_BASE, 16 * MiB);
    // Generic stub for FIRC/FXOSC/MC_CGM range (0x402D0000-0x402DBFFF, 48KB).
    // Still needs _overlap since it sits inside the 16MiB catch-all above.
    memory_region_init_io(&s->clkgen_stub, NULL, &s32k389_clkgen_ops, s,
                          "s32k389.clkgen_stub", S32K3_CLKGEN_STUB_SIZE);
    memory_region_add_subregion_overlap(system_memory, S32K3_CLKGEN_STUB_BASE,
                                        &s->clkgen_stub, 1);
    // Real MC_ME peripheral (0x402DC000-0x402DFFFF, 16KB) - immediately
    // follows the clkgen stub with no overlap between the two of them, but
    // still needs _overlap against the 16MiB catch-all above.
    memory_region_init_io(&s->mc_me, NULL, &s32k389_mc_me_ops, s,
                          "s32k389.mc_me", S32K3_MC_ME_SIZE);
    memory_region_add_subregion_overlap(system_memory, S32K3_MC_ME_BASE,
                                        &s->mc_me, 1);
 
    // Enabling semihosting for guest BKPT operations
    qemu_semihosting_enable();
 
    // Load firmware - FLASH_SIZE covers the full 12MB contiguous flash
    // region (flash0-flash7), not just the first block, since firmware can
    // legitimately span multiple physical flash blocks.
    armv7m_load_kernel(s->armv7m.cpu, machine->kernel_filename, INT_CODE_FLASH0_BASE, FLASH_SIZE);
}
 
static void s32k389_class_init(ObjectClass* oc, const void* data) {
    MachineClass* mc = MACHINE_CLASS(oc);
    mc->desc = "NXP S32K389 Development Board (Cortex-M7)";
    mc->init = s32k389_init;
    mc->default_cpus = 1;
    mc->min_cpus = 1;
    mc->max_cpus = 1;
    mc->default_ram_size = SRAM_SIZE;
}
 
static void s32k389_instance_init(Object *obj)
{
    S32K389State *s = S32K389(obj);
 
    /* S32K389_CAN_COUNT is 12 (vs. 6 on the earlier S32K388-derived model),
     * so register a link property "canbusN" for each of the 12 instances. */
    for (int i = 0; i < S32K389_CAN_COUNT; i++) {
        g_autofree char *prop_name = g_strdup_printf("canbus%d", i);
        object_property_add_link(obj, prop_name, TYPE_CAN_BUS,
                                 (Object **)&s->canbus[i],
                                 object_property_allow_set_link, 0);
    }
}
 
static const TypeInfo s32k389_type = {
    .name = TYPE_S32K389,
    .parent = TYPE_MACHINE,
    .instance_size = sizeof(S32K389State),
    .instance_init = s32k389_instance_init,
    .class_init = s32k389_class_init,
    .interfaces = arm_machine_interfaces,
};
 
// Register machine type
static void s32k389_machine_init(void) {
    qemu_log_mask(CPU_LOG_INT, "Registering S32K389 machine type\n");
    type_register_static(&s32k389_type);
}
 
type_init(s32k389_machine_init);
 
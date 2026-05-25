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
#include "hw/arm/s32k344.h"
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


static void s32k344_init(MachineState* machine) {
    S32K344State* s = S32K344(machine);
    Error* error_local = NULL;
    //DeviceState* dev;
    
    qemu_log_mask(CPU_LOG_INT, "Initializing S32K344\n");

    // Get system memory
    MemoryRegion* system_memory = get_system_memory();

    // Initialize memory
    qemu_log_mask(CPU_LOG_INT, "Initializing memory regions\n");
    // ITCM
    memory_region_init_ram(&s->itcm, NULL, "S32K344.itcm", INT_ITCM_SIZE, &error_fatal);
    memory_region_add_subregion(system_memory, INT_ITCM_BASE, &s->itcm);

    // DTCM
    memory_region_init_ram(&s->dtcm, NULL, "S32K344.dtcm", INT_DTCM_SIZE, &error_fatal);
    memory_region_add_subregion(system_memory, INT_DTCM_BASE, &s->dtcm);

    // DTCM Stack
    memory_region_init_ram(&s->dtcm_stack, NULL, "S32K344.dtcm_stack", INT_DTCM_STACK_SIZE, &error_fatal);
    memory_region_add_subregion(system_memory, INT_DTCM_STACK_BASE, &s->dtcm_stack);

    // Flash
    memory_region_init_ram(&s->C0flash, NULL, "S32K344.C0flash", INT_CODE_FLASH0_SIZE, &error_fatal);
    memory_region_add_subregion(system_memory, INT_CODE_FLASH0_BASE, &s->C0flash);
    memory_region_init_rom(&s->C1flash, NULL, "S32K344.C1flash", INT_CODE_FLASH1_SIZE, &error_fatal);
    memory_region_add_subregion(system_memory, INT_CODE_FLASH1_BASE, &s->C1flash);
    memory_region_init_rom(&s->C2flash, NULL, "S32K344.C2flash", INT_CODE_FLASH2_SIZE, &error_fatal);
    memory_region_add_subregion(system_memory, INT_CODE_FLASH2_BASE, &s->C2flash);
    memory_region_init_rom(&s->C3flash, NULL, "S32K344.C3flash", INT_CODE_FLASH3_SIZE, &error_fatal);
    memory_region_add_subregion(system_memory, INT_CODE_FLASH3_BASE, &s->C3flash);
    memory_region_init_rom(&s->Dflash, NULL, "S32K344.Dflash", INT_DATA_FLASH_SIZE, &error_fatal);
    memory_region_add_subregion(system_memory, INT_DATA_FLASH_BASE, &s->Dflash);
    memory_region_init_rom(&s->UNVMflash, NULL, "S32K344.UNVMflash", INT_UTEST_NVM_FLASH_SIZE, &error_fatal);
    memory_region_add_subregion(system_memory, INT_UTEST_NVM_FLASH_BASE, &s->UNVMflash);

    // SRAM
    memory_region_init_ram(&s->sram_standby, NULL, "S32K344.sram_standby", INT_SRAM_STANDBY_SIZE, &error_fatal);
    memory_region_add_subregion(system_memory, INT_SRAM_STANDBY_BASE, &s->sram_standby);
    memory_region_init_ram(&s->sram0, NULL, "S32K344.sram0", INT_SRAM_0_SIZE, &error_fatal);
    memory_region_add_subregion(system_memory, INT_SRAM_0_BASE, &s->sram0);
    memory_region_init_ram(&s->sram1, NULL, "S32K344.sram1", INT_SRAM_1_SIZE, &error_fatal);
    memory_region_add_subregion(system_memory, INT_SRAM_1_BASE, &s->sram1);

    qemu_log_mask(CPU_LOG_INT, "Memory regions successfully initialized\n");

    // Initialize system clock
    s->sysclk = clock_new(OBJECT(machine), "SYSCLK");
    clock_set_hz(s->sysclk, S32K3_SYSCLK_FREQ);

    // Initialize ARM core
    object_initialize_child(OBJECT(machine), "armv7m", &s->armv7m, TYPE_ARMV7M);

    // Configure CPU
    qdev_prop_set_string(DEVICE(&s->armv7m), "cpu-type", ARM_CPU_TYPE_NAME("cortex-m7"));
    qdev_prop_set_uint32(DEVICE(&s->armv7m), "init-svtor", INT_CODE_FLASH0_BASE);
    qdev_prop_set_uint32(DEVICE(&s->armv7m), "init-nsvtor", INT_CODE_FLASH0_BASE);
    qdev_prop_set_uint8(DEVICE(&s->armv7m), "num-prio-bits", 4);
    qdev_prop_set_uint32(DEVICE(&s->armv7m), "num-irq", 240);

    // Set up system connections
    object_property_set_link(OBJECT(&s->armv7m), "memory", OBJECT(system_memory), &error_abort);
    qdev_connect_clock_in(DEVICE(&s->armv7m), "cpuclk", s->sysclk);

    // Implement system bus device
    sysbus_realize(SYS_BUS_DEVICE(&s->armv7m), &error_local);

    // Map any missing S32K3 peripheral region used by firmware
    create_unimplemented_device("S32K344.unimplemented_periph", 0x4028c000, 0x1000);

    // Enabling semihosting for guest BKPT operations
    qemu_semihosting_enable();

    // Load firmware
    armv7m_load_kernel(s->armv7m.cpu, machine->kernel_filename, INT_CODE_FLASH0_BASE, INT_CODE_FLASH0_SIZE);
}

static void s32k344_class_init(ObjectClass* oc, const void* data) {
    MachineClass* mc = MACHINE_CLASS(oc);
    mc->desc = "NXP S32K344 Development Board (Cortex-M7)";
    mc->init = s32k344_init;
    mc->default_cpus = 1;
    mc->min_cpus = 1;
    mc->max_cpus = 1;
    mc->default_ram_size = SRAM_SIZE;
}

static const TypeInfo s32k344_type = {
    .name = TYPE_S32K344,
    .parent = TYPE_MACHINE,
    .instance_size = sizeof(S32K344State),
    .class_init = s32k344_class_init,
    .interfaces = arm_machine_interfaces,
};

// Register machine type
static void s32k344_machine_init(void) {
    qemu_log_mask(CPU_LOG_INT, "Registering S32K344 machine type\n");
    type_register_static(&s32k344_type);
}

type_init(s32k344_machine_init)
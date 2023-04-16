/*
 * AppleMobileDispH12P
 *
 * Copyright (c) 2023 ChefKiss Inc. Licensed under the Thou Shalt Not Profit License version 1.0. See LICENSE.TSNPL
 * for details.
 *
 */
#ifndef HW_FB_APPLE_H12P_H
#define HW_FB_APPLE_H12P_H

#include "hw/sysbus.h"
#include "sysemu/dma.h"
#include "ui/console.h"
#include "qom/object.h"

#define T8030_DISPLAY_BASE      (0x8f7fb4000)
#define T8030_DISPLAY_SIZE      (35 * 1024 * 1024)

#define TYPE_APPLE_H12P "apple-h12p"
OBJECT_DECLARE_SIMPLE_TYPE(AppleH12PState, APPLE_H12P);

enum {
    REG_M3_FRAME_REPEAT = 0xa2d8,
    REG_M3_EVENT_CONFIG = 0x8548,
    REG_UPPIPE_VER = 0x46020,
    REG_UPPIPE_IDLE_SUBFRAMES = 0x48050,
    REG_GENPIPE0_IDK = 0x50004,
    REG_GENPIPE0_FRAME_SIZE = 0x50080,
    REG_GENPIPE1_IDK = 0x58004,
    REG_GENPIPE1_FRAME_SIZE = 0x58080,
};

struct AppleH12PState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    // MemoryRegion iomem;
    MemoryRegion *dma_mr;
    AddressSpace dma_as;
    MemoryRegionSection vram_section;
    QemuConsole *console;

    /* Configuration data for the FB */
    uint32_t width, height;
    size_t cnt;
    // dma_addr_t dma_addr[0x1E];

    uint8_t regs[0xBC2D000];
};

void apple_h12p_create(MachineState *machine);

#endif /* HW_FB_APPLE_H12P_H */

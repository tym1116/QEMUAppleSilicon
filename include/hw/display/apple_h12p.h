/*
 * AppleMobileDispH12P
 *
 * Copyright (c) 2023 ChefKiss Inc. Licensed under the Thou Shalt Not Profit License version 1.0. See LICENSE.TSNPL
 * for details.
 *
 */
#ifndef HW_FB_APPLE_H12P_H
#define HW_FB_APPLE_H12P_H

#include "sysemu/dma.h"
#include "ui/console.h"
#include "qom/object.h"
#include "hw/sysbus.h"
#include "hw/irq.h"

#define T8030_DISPLAY_BASE (0x8F7FB4000)
#define T8030_DISPLAY_SIZE (35 * 1024 * 1024)

#define TYPE_APPLE_H12P "apple-h12p"
OBJECT_DECLARE_SIMPLE_TYPE(AppleH12PState, APPLE_H12P);

#define REG_M3_FRAME_REPEAT       (0xA2D8)
#define REG_M3_EVENT_CONFIG       (0x8548)
#define REG_UPPIPE_VER            (0x46020)
#define REG_UPPIPE_IDLE_SUBFRAMES (0x48050)
#define REG_UPPIPE_FRAME_SIZE     (0x4603C)
#define REG_GENPIPE0_IDK          (0x50004)
#define REG_GENPIPE0_FRAME_SIZE   (0x50080)
#define REG_UP_CONFIG_FRAME_SIZE  (0x24603C)

#define IOMFB_MBOX_IRQ_APT (1)
#define IOMFB_MBOX_IRQ_PCC (8)

#define IOMFB_MBOX_REG_BASE_APT (0x10000)
#define IOMFB_MBOX_REG_BASE_PCC (0xE0000)

#define IOMFB_MBOX_INT_FILTER (0x2C)

#define IOMFB_MBOX_INT_FILTER_INBOX_EMPTY  (1UL << 0)
#define IOMFB_MBOX_INT_FILTER_OUTBOX_EMPTY (1UL << 3)

#define IOMFB_MBOX_INBOX  (0x30)
#define IOMFB_MBOX_OUTBOX (0x3C)

#define IOMFB_MBOX_CTL   (0x00)
#define IOMFB_MBOX_CTL_ENABLE (1UL << 17)
#define IOMFB_MBOX_WRITE (0x04)
#define IOMFB_MBOX_READ  (0x08)

struct IOMFBMboxState {
    uint32_t int_filter;
    uint32_t inbox_ctl;
    uint32_t inbox_read;
    uint32_t inbox_write;
    uint32_t outbox_ctl;
    uint32_t outbox_read;
    uint32_t outbox_write;
    qemu_irq irq;
};

typedef struct IOMFBMboxState IOMFBMboxState;

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

    qemu_irq irqs[9];

    IOMFBMboxState apt_mbox;
    IOMFBMboxState pcc_mbox;
};

void apple_h12p_create(MachineState *machine);

#endif /* HW_FB_APPLE_H12P_H */

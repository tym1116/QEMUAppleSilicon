/*
 * Apple H12P Display Controller.
 *
 * Copyright (c) 2023 Visual Ehrmanntraut.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */
#ifndef HW_FB_APPLE_H12P_H
#define HW_FB_APPLE_H12P_H

#include "qemu/osdep.h"
#include "hw/irq.h"
#include "hw/sysbus.h"
#include "qemu/timer.h"
#include "qom/object.h"
#include "sysemu/dma.h"
#include "ui/console.h"

#define T8030_DISPLAY_BASE (0x8F7FB4000)
#define T8030_DISPLAY_SIZE (67 * 1024 * 1024)

#define TYPE_APPLE_H12P "apple-h12p"
OBJECT_DECLARE_SIMPLE_TYPE(AppleH12PState, APPLE_H12P);

#define REG_UPPIPE_VER (0x46020)
#define REG_UPPIPE_FRAME_SIZE (0x4603C)
#define REG_GENPIPE0_BLACK_FRAME (0x50004)
#define REG_GENPIPE1_BLACK_FRAME (0x58004)
#define REG_GENPIPE0_PIXEL_FORMAT (0x5001C)
#define REG_GENPIPE1_PIXEL_FORMAT (0x5801C)
#define GENPIPE_DFB_PIXEL_FORMAT_BGRA (0x11110)
#define REG_GENPIPE0_PLANE_START (0x50030)
#define REG_GENPIPE0_PLANE_END (0x50040)
#define REG_GENPIPE0_PLANE_STRIDE (0x50060)
#define REG_GENPIPE1_PLANE_START (0x58030)
#define REG_GENPIPE1_PLANE_END (0x58040)
#define REG_GENPIPE1_PLANE_STRIDE (0x58060)
#define REG_GENPIPE0_FRAME_SIZE (0x50080)
#define REG_GENPIPE1_FRAME_SIZE (0x58080)
#define REG_UPPIPE_INT_FILTER (0x45818)
#define REG_PCC_SOFT_RESET (0xB0130)

struct AppleH12PState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    uint32_t width, height;
    MemoryRegion up_regs, vram;
    MemoryRegion *dma_mr;
    AddressSpace dma_as;
    MemoryRegionSection vram_section;
    qemu_irq irqs[9];
    uint32_t uppipe_int_filter, genpipe0_plane_start, genpipe0_plane_end,
        genpipe0_plane_stride, genpipe1_plane_start, genpipe1_plane_end,
        genpipe1_plane_stride;
    bool frame_processed;
    uint8_t regs[0x200000];
    QemuConsole *console;
};

void apple_h12p_create(MachineState *machine);

#endif /* HW_FB_APPLE_H12P_H */

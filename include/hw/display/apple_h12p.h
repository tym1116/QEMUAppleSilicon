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

#define H12P_UPPIPE_INT_FILTER (0x45818)
#define H12P_UPPIPE_VER (0x46020)
#define UPPIPE_VER_A1 (0x70045)
#define H12P_UPPIPE_FRAME_SIZE (0x4603C)

#define H12P_GENPIPE_BASE (0x50000)
#define H12P_GENPIPE_REG_SIZE (0x08000)
#define H12P_GP_CONFIG_CONTROL (0x00004)
#define GP_CONFIG_CONTROL_ENABLED (1U << 31)
#define GP_CONFIG_CONTROL_BLACK_FRAME (1U << 30)
#define H12P_GENPIPE_PIXEL_FORMAT (0x0001C)
#define GENPIPE_DFB_PIXEL_FORMAT_BGRA (0x11110)
#define H12P_GENPIPE_PLANE_START (0x00030)
#define H12P_GENPIPE_PLANE_END (0x00040)
#define H12P_GENPIPE_PLANE_STRIDE (0x00060)
#define H12P_GENPIPE_FRAME_SIZE (0x00080)

#define H12P_PCC_SOFT_RESET (0xB0130)

struct GenPipeState {
    size_t index;
    uint32_t width, height;
    uint32_t config_control;
    uint32_t plane_start, plane_end, plane_stride;
};
typedef struct GenPipeState GenPipeState;

#define H12P_GENPIPE_BASE_FOR(i) (H12P_GENPIPE_BASE + i * H12P_GENPIPE_REG_SIZE)

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
    uint32_t uppipe_int_filter;
    GenPipeState genpipe0, genpipe1;
    bool frame_processed;
    QemuConsole *console;
};

void apple_h12p_create(MachineState *machine);

#endif /* HW_FB_APPLE_H12P_H */

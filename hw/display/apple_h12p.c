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

#include "qemu/osdep.h"
#include "hw/arm/apple_dart.h"
#include "hw/arm/t8030.h"
#include "hw/display/apple_h12p.h"
#include "hw/qdev-properties.h"
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "qom/object.h"
#include "ui/console.h"
#include "ui/pixel_ops.h"
#include "framebuffer.h"

static bool h12p_genpipe_write(GenPipeState *s, hwaddr addr, uint64_t data)
{
    if (addr < H12P_GENPIPE_BASE_FOR(s->index)) {
        return false;
    }
    switch (addr - H12P_GENPIPE_BASE_FOR(s->index)) {
    // case H12P_GENPIPE_BLACK_FRAME:
    //     s->black_frame = (uint32_t)data;
    //     return true;
    case H12P_GENPIPE_PLANE_START:
        s->plane_start = (uint32_t)data;
        info_report("[H12P] GenPipe %zu: Plane Start <- 0x" HWADDR_FMT_plx,
                    s->index, data);
        return true;
    case H12P_GENPIPE_PLANE_END:
        s->plane_end = (uint32_t)data;
        info_report("[H12P] GenPipe %zu: Plane End <- 0x" HWADDR_FMT_plx,
                    s->index, data);
        return true;
    case H12P_GENPIPE_PLANE_STRIDE:
        s->plane_stride = (uint32_t)data;
        info_report("[H12P] GenPipe %zu: Plane Stride <- 0x" HWADDR_FMT_plx,
                    s->index, data);
        return true;
    default:
        break;
    }
    return false;
}

static bool h12p_genpipe_read(GenPipeState *s, hwaddr addr, uint64_t *data)
{
    if (addr < H12P_GENPIPE_BASE_FOR(s->index)) {
        return false;
    }
    switch (addr - H12P_GENPIPE_BASE_FOR(s->index)) {
    // case H12P_GENPIPE_BLACK_FRAME:
    //     *data = s->black_frame;
    //     return true;
    case H12P_GENPIPE_PLANE_START:
        *data = s->plane_start;
        return true;
    case H12P_GENPIPE_PLANE_END:
        *data = s->plane_end;
        return true;
    case H12P_GENPIPE_PLANE_STRIDE:
        *data = s->plane_stride;
        return true;
    case H12P_GENPIPE_PIXEL_FORMAT:
        *data = GENPIPE_DFB_PIXEL_FORMAT_BGRA;
        return true;
    case H12P_GENPIPE_FRAME_SIZE:
        *data = (s->width << 16) | s->height;
        return true;
    default:
        break;
    }
    return false;
}

static uint8_t *h12p_genpipe_read_fb(GenPipeState *s, AddressSpace *dma_as,
                                     uint32_t plane_stride, size_t *size_out)
{
    if (s->plane_start && s->plane_end && s->plane_stride && plane_stride) {
        size_t size = s->plane_end - s->plane_start;
        uint8_t *buf = g_malloc(size);
        if (dma_memory_read(dma_as, s->plane_start, buf, size,
                            MEMTXATTRS_UNSPECIFIED) == MEMTX_OK) {
            *size_out = size;
            return buf;
        }
    }
    *size_out = 0;
    return NULL;
}

static bool h12p_genpipe_init(GenPipeState *s, size_t index, uint32_t width,
                              uint32_t height)
{
    memset(s, 0, sizeof(*s));
    s->index = index;
    s->width = width;
    s->height = height;
    return true;
}


static void h12p_up_write(void *opaque, hwaddr addr, uint64_t data,
                          unsigned size)
{
    AppleH12PState *s = APPLE_H12P(opaque);
    if (addr >= 0x200000) {
        addr -= 0x200000;
    }
    if (h12p_genpipe_write(&s->genpipe0, addr, data)) {
        info_report("[H12P] GenPipe 0: 0x" HWADDR_FMT_plx
                    " <- 0x" HWADDR_FMT_plx,
                    addr, data);
        return;
    }
    if (h12p_genpipe_write(&s->genpipe1, addr, data)) {
        info_report("[H12P] GenPipe 1: 0x" HWADDR_FMT_plx
                    " <- 0x" HWADDR_FMT_plx,
                    addr, data);
        return;
    }
    info_report("[H12P] 0x" HWADDR_FMT_plx " <- 0x" HWADDR_FMT_plx, addr, data);
    switch (addr) {
    case H12P_UPPIPE_INT_FILTER:
        s->uppipe_int_filter &= ~(uint32_t)data;
        s->frame_processed = false;
        break;
    case H12P_PCC_SOFT_RESET:
        info_report("[H12P] PCC SOFT RESET!");
        break;
    default:
        break;
    }
}

static uint64_t h12p_up_read(void *opaque, hwaddr addr, unsigned size)
{
    AppleH12PState *s = APPLE_H12P(opaque);
    uint64_t ret = 0;
    if (addr >= 0x200000) {
        addr -= 0x200000;
    }
    if (h12p_genpipe_read(&s->genpipe0, addr, &ret)) {
        info_report("[H12P] GenPipe 0: 0x" HWADDR_FMT_plx
                    " -> 0x" HWADDR_FMT_plx,
                    addr, ret);
        return ret;
    }
    if (h12p_genpipe_read(&s->genpipe1, addr, &ret)) {
        info_report("[H12P] GenPipe 1: 0x" HWADDR_FMT_plx
                    " -> 0x" HWADDR_FMT_plx,
                    addr, ret);
        return ret;
    }
    switch (addr) {
    case H12P_UPPIPE_VER:
        ret = 0x70045;
        break;
    case H12P_UPPIPE_FRAME_SIZE:
        ret = (s->width << 16) | s->height;
        break;
    case H12P_UPPIPE_INT_FILTER:
        ret = s->uppipe_int_filter;
        qemu_irq_lower(s->irqs[0]);
        break;
    default:
        break;
    }
    info_report("[H12P] 0x" HWADDR_FMT_plx " -> 0x" HWADDR_FMT_plx, addr, ret);
    return ret;
}

static const MemoryRegionOps h12p_up_ops = {
    .write = h12p_up_write,
    .read = h12p_up_read,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl.min_access_size = 4,
    .impl.max_access_size = 4,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
    .valid.unaligned = false,
};

void apple_h12p_create(MachineState *machine)
{
    T8030MachineState *tms = T8030_MACHINE(machine);
    DeviceState *dev = qdev_new(TYPE_APPLE_H12P);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    AppleH12PState *s = APPLE_H12P(sbd);
    tms->video.v_baseAddr = T8030_DISPLAY_BASE;
    tms->video.v_rowBytes = s->width * 4;
    tms->video.v_width = s->width;
    tms->video.v_height = s->height;
    tms->video.v_depth = 32 | ((2 - 1) << 16);
    tms->video.v_display = 1;

    if (xnu_contains_boot_arg(machine->kernel_cmdline, "-s", false) ||
        xnu_contains_boot_arg(machine->kernel_cmdline, "-v", false)) {
        tms->video.v_display = 0;
    }

    DTBNode *armio = find_dtb_node(tms->device_tree, "arm-io");
    assert(armio);
    DTBNode *child = find_dtb_node(armio, "disp0");
    assert(child);
    assert(set_dtb_prop(child, "display-target", 15, "DisplayTarget5"));
    uint32_t dispTimingInfo[] = { 0x33C, 0x90, 0x1, 0x1, 0x700, 0x1, 0x1, 0x1 };
    assert(set_dtb_prop(child, "display-timing-info", sizeof(dispTimingInfo),
                        &dispTimingInfo));
    uint32_t data = 0xD;
    assert(set_dtb_prop(child, "bics-param-set", sizeof(data), &data));
    uint32_t dot_pitch = 326;
    assert(set_dtb_prop(child, "dot-pitch", sizeof(dot_pitch), &dot_pitch));
    assert(set_dtb_prop(child, "function-brightness_update", 0, ""));

    DTBProp *prop = find_dtb_prop(child, "reg");
    assert(prop);
    uint64_t *reg = (uint64_t *)prop->value;
    memory_region_init_io(&s->up_regs, OBJECT(sbd), &h12p_up_ops, sbd,
                          "up.regs", reg[1]);
    sysbus_init_mmio(sbd, &s->up_regs);
    sysbus_mmio_map(sbd, 0, tms->soc_base_pa + reg[0]);
    object_property_add_const_link(OBJECT(sbd), "up.regs", OBJECT(&s->up_regs));

    prop = find_dtb_prop(child, "interrupts");
    assert(prop);
    uint32_t *ints = (uint32_t *)prop->value;

    for (size_t i = 0; i < prop->length / sizeof(uint32_t); i++) {
        sysbus_init_irq(sbd, &s->irqs[i]);
        sysbus_connect_irq(sbd, i, qdev_get_gpio_in(DEVICE(tms->aic), ints[i]));
    }

    AppleDARTState *dart = APPLE_DART(
        object_property_get_link(OBJECT(machine), "dart-disp0", &error_fatal));
    assert(dart);
    child = find_dtb_node(armio, "dart-disp0");
    assert(child);
    child = find_dtb_node(child, "mapper-disp0");
    assert(child);
    prop = find_dtb_prop(child, "reg");
    assert(prop);
    s->dma_mr =
        MEMORY_REGION(apple_dart_iommu_mr(dart, *(uint32_t *)prop->value));
    assert(s->dma_mr);
    assert(object_property_add_const_link(OBJECT(sbd), "dma_mr",
                                          OBJECT(s->dma_mr)));
    address_space_init(&s->dma_as, s->dma_mr, "disp0.dma");

    memory_region_init_ram(&s->vram, OBJECT(sbd), "vram", T8030_DISPLAY_SIZE,
                           &error_fatal);
    memory_region_add_subregion_overlap(tms->sysmem, tms->video.v_baseAddr,
                                        &s->vram, 1);
    object_property_add_const_link(OBJECT(sbd), "vram", OBJECT(&s->vram));
    object_property_add_child(OBJECT(machine), "disp0", OBJECT(sbd));

    sysbus_realize_and_unref(sbd, &error_fatal);
}

static void h12p_draw_row(void *opaque, uint8_t *dest, const uint8_t *src,
                          int width, int dest_pitch)
{
    while (width--) {
        /* Load using endian-safe loads */
        uint32_t colour = ldl_le_p(src);
        /* Increment source pointer */
        src += sizeof(colour);

        /* Blit it to the display output now that it's converted */
        /* FIXME this might not be endian-safe but the rest should be */
        memcpy(dest, &colour, sizeof(colour));
        dest += sizeof(colour);
    }
}

static void h12p_gfx_update(void *opaque)
{
    AppleH12PState *s = APPLE_H12P(opaque);
    DisplaySurface *surface = qemu_console_surface(s->console);

    int stride = s->width * sizeof(uint32_t);

    if (!s->genpipe0.plane_start || !s->genpipe0.plane_end) {
        int first = 0, last = 0;

        if (!s->vram_section.mr) {
            framebuffer_update_memory_section(&s->vram_section, &s->vram, 0,
                                              s->height, stride);
        }
        framebuffer_update_display(surface, &s->vram_section, s->width,
                                   s->height, stride, stride, 0, 0,
                                   h12p_draw_row, s, &first, &last);
        if (first >= 0) {
            dpy_gfx_update(s->console, 0, first, s->width, last - first + 1);
        }

        return;
    }

    if (!s->frame_processed) {
        size_t size0 = 0;
        g_autofree uint8_t *buf0 = h12p_genpipe_read_fb(
            &s->genpipe0, &s->dma_as, s->genpipe0.plane_stride, &size0);
        size_t size1 = 0;
        g_autofree uint8_t *buf1 = h12p_genpipe_read_fb(
            &s->genpipe1, &s->dma_as, s->genpipe1.plane_stride, &size1);

        uint8_t *dest = surface_data(surface);
        for (size_t i = 0; i < s->height; i++) {
            if (size0 && buf0 != NULL)
                h12p_draw_row(s, dest + i * stride, buf0 + i * stride, s->width,
                              0);
            if (size1 && buf1 != NULL)
                h12p_draw_row(s, dest + i * stride, buf1 + i * stride, s->width,
                              0);
        }

        dpy_gfx_update_full(s->console);
        s->uppipe_int_filter |= (1UL << 10) | (1UL << 19) | (1UL << 20);
        qemu_irq_raise(s->irqs[0]);
        s->frame_processed = true;
    }
}

static const GraphicHwOps apple_h12p_ops = {
    .gfx_update = h12p_gfx_update,
};

static void apple_h12p_realize(DeviceState *dev, Error **errp)
{
    AppleH12PState *s = APPLE_H12P(dev);

    s->uppipe_int_filter = 0;
    s->frame_processed = false;
    h12p_genpipe_init(&s->genpipe0, 0, s->width, s->height);
    h12p_genpipe_init(&s->genpipe1, 1, s->width, s->height);
    s->console = graphic_console_init(dev, 0, &apple_h12p_ops, s);
    qemu_console_resize(s->console, s->width, s->height);
}

static const VMStateDescription vmstate_apple_h12p = {
    .name = TYPE_APPLE_H12P,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields =
        (VMStateField[]){
            VMSTATE_UINT32(width, AppleH12PState),
            VMSTATE_UINT32(height, AppleH12PState),
            VMSTATE_END_OF_LIST(),
        },
};

static Property apple_h12p_props[] = {
    // iPhone 4/4S
    DEFINE_PROP_UINT32("width", AppleH12PState, width, 640),
    DEFINE_PROP_UINT32("height", AppleH12PState, height, 960),
    // iPhone 11
    // DEFINE_PROP_UINT32("width", AppleH12PState, width, 828),
    // DEFINE_PROP_UINT32("height", AppleH12PState, height, 1792),
    DEFINE_PROP_END_OF_LIST()
};

static void apple_h12p_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    set_bit(DEVICE_CATEGORY_DISPLAY, dc->categories);
    device_class_set_props(dc, apple_h12p_props);
    dc->realize = apple_h12p_realize;
    dc->vmsd = &vmstate_apple_h12p;
}

static const TypeInfo apple_h12p_type_info = {
    .name = TYPE_APPLE_H12P,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(AppleH12PState),
    .class_init = apple_h12p_class_init,
};

static void apple_h12p_register_types(void)
{
    type_register_static(&apple_h12p_type_info);
}

type_init(apple_h12p_register_types);

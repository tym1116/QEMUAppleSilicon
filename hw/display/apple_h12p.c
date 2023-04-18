/*
 * AppleMobileDispH12P
 *
 * Copyright (c) 2023 ChefKiss Inc. Licensed under the Thou Shalt Not Profit License version 1.0. See LICENSE.TSNPL
 * for details.
 *
 */

#include "qemu/osdep.h"
#include "hw/display/apple_h12p.h"
#include "hw/qdev-properties.h"
#include "migration/vmstate.h"
#include "qom/object.h"
#include "ui/pixel_ops.h"
#include "ui/console.h"
#include "qapi/error.h"
#include "framebuffer.h"
#include "hw/arm/t8030.h"
#include "hw/arm/apple_dart.h"

static void h12p_up_write(void *opaque, hwaddr addr, uint64_t data, unsigned size)
{
    AppleH12PState *s = APPLE_H12P(opaque);
    switch (addr) {
        case REG_GENPIPE0_PLANE_START:
            s->genpipe0_plane_start = (uint32_t)data;
            break;
        case REG_GENPIPE0_PLANE_END:
            s->genpipe0_plane_end = (uint32_t)data;
            qemu_irq_lower(s->irqs[0]);
            break;
        default:
            break;
    }
            info_report("h12p_up_write: 0x" TARGET_FMT_plx " <- 0x" TARGET_FMT_plx, addr, data);
}

static uint64_t h12p_up_read(void *opaque, hwaddr addr, unsigned size)
{
    AppleH12PState *s = APPLE_H12P(opaque);
    uint64_t ret = 0;
    switch (addr) {
        case REG_UPPIPE_VER:
            ret = 0x70045;
            break;
        case REG_GENPIPE0_BLACK_FRAME:
        case REG_GENPIPE1_BLACK_FRAME:
            ret = -1;
            break;
        case REG_UPPIPE_FRAME_SIZE:
            ret = (s->width << 16) | s->height;
            break;
        case REG_GENPIPE0_PIXEL_FORMAT:
        case REG_GENPIPE1_PIXEL_FORMAT:
            ret = GENPIPE_DFB_PIXEL_FORMAT_BGRA;
            break;
        case REG_GENPIPE0_PLANE_START:
            ret = s->genpipe0_plane_start;
            break;
        case REG_GENPIPE0_PLANE_END:
            ret = s->genpipe0_plane_end;
            break;
        case REG_GENPIPE0_FRAME_SIZE:
        case REG_GENPIPE1_FRAME_SIZE:
        case REG_UP_CONFIG_FRAME_SIZE:
            ret = ((s->width * 4) << 16) | s->height;
            break;
        default:
            break;
    }
    info_report("h12p_up_read: 0x" TARGET_FMT_plx " -> 0x" TARGET_FMT_plx, addr, ret);
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

static void apple_h12p_timer_int(void *opaque)
{
    AppleH12PState *s = APPLE_H12P(opaque);
    qemu_irq_raise(s->irqs[0]);
    // qemu_irq_raise(s->irqs[2]);
    // qemu_irq_raise(s->irqs[3]);
    // qemu_irq_raise(s->irqs[4]);
    // qemu_irq_raise(s->irqs[6]);
    timer_mod(s->timer, qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + 0x33C);
    qemu_irq_lower(s->irqs[0]);
    // qemu_irq_lower(s->irqs[2]);
    // qemu_irq_lower(s->irqs[3]);
    // qemu_irq_lower(s->irqs[4]);
    // qemu_irq_lower(s->irqs[6]);
}

void apple_h12p_create(MachineState *machine)
{
    T8030MachineState *tms = T8030_MACHINE(machine);
    DeviceState *dev = qdev_new(TYPE_APPLE_H12P);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    AppleH12PState *s = APPLE_H12P(sbd);

    DTBNode *armio = find_dtb_node(tms->device_tree, "arm-io");
    assert(armio);
    DTBNode *child = find_dtb_node(armio, "disp0");
    assert(child);
    assert(set_dtb_prop(child, "display-target", 15, "DisplayTarget5"));
    uint32_t dispTimingInfo[] = {0x33C, 0x90, 0x1, 0x1, 0x700, 0x1, 0x1, 0x1};
    assert(set_dtb_prop(child, "display-timing-info", 8 * sizeof(uint32_t), &dispTimingInfo));
    uint32_t data = 0xD;
    assert(set_dtb_prop(child, "bics-param-set", 4, &data));

    DTBProp *prop = find_dtb_prop(child, "reg");
    assert(prop);
    MemoryRegion *h12p_mem = g_new(MemoryRegion, 1);
    uint64_t *reg = (uint64_t *)prop->value;
    memory_region_init_io(h12p_mem, OBJECT(sbd), &h12p_up_ops, sbd, "up.regs", reg[1]);
    sysbus_init_mmio(sbd, h12p_mem);
    sysbus_mmio_map(sbd, 0, tms->soc_base_pa + reg[0]);
    object_property_add_const_link(OBJECT(sbd), "up.regs", OBJECT(h12p_mem));

    prop = find_dtb_prop(child, "interrupts");
    assert(prop);
    uint32_t *ints = (uint32_t *)prop->value;

    for (size_t i = 0; i < prop->length / sizeof(uint32_t); i++) {
        sysbus_init_irq(sbd, &s->irqs[0]);
        sysbus_connect_irq(sbd, i, qdev_get_gpio_in(DEVICE(tms->aic), ints[i]));
    }

    s->timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, apple_h12p_timer_int, s);

    AppleDARTState *dart = APPLE_DART(object_property_get_link(OBJECT(machine), "dart-disp0", &error_fatal));
    assert(dart);
    child = find_dtb_node(armio, "dart-disp0");
    assert(child);
    child = find_dtb_node(child, "mapper-disp0");
    assert(child);
    prop = find_dtb_prop(child, "reg");
    assert(prop);
    s->dma_mr = MEMORY_REGION(apple_dart_iommu_mr(dart, *(uint32_t *)prop->value));
    assert(s->dma_mr);
    assert(object_property_add_const_link(OBJECT(sbd), "dma_mr", OBJECT(s->dma_mr)));
    address_space_init(&s->dma_as, s->dma_mr, "disp0");

    object_property_add_child(OBJECT(machine), "h12p", OBJECT(sbd));

    sysbus_realize_and_unref(sbd, &error_fatal);
}

static void fb_draw_row(void *opaque, uint8_t *dest, const uint8_t *src, int width, int dest_pitch)
{
    while (width--) {
        /* Load using endian-safe loads */
        uint32_t color = ldl_le_p(src);
        /* Increment source pointer */
        src += dest_pitch;

        /* Blit it to the display output now that it's converted */
        /* FIXME this might not be endian-safe but the rest should be */
        memcpy(dest, &color, sizeof(color));
        dest += dest_pitch;
    }
}

static void fb_gfx_update(void *opaque)
{
    AppleH12PState *s = APPLE_H12P(opaque);
    DisplaySurface *surface = qemu_console_surface(s->console);

    if (!s->genpipe0_plane_start || !s->genpipe0_plane_end) {
        return;
    }

    size_t size = s->genpipe0_plane_end - s->genpipe0_plane_start;
    uint8_t *buf = g_malloc(size);
    if (dma_memory_read(&s->dma_as, s->genpipe0_plane_start, buf, size, MEMTXATTRS_UNSPECIFIED) != MEMTX_OK) {
        g_free(buf);
        return;
    }

    uint8_t *dest = surface_data(surface);
    for (size_t i = 0; i < s->height; i++) {
        fb_draw_row(s, dest, buf + i * s->width * 4, s->width, 4);
        dest += s->width;
    }
    g_free(buf);

    dpy_gfx_update_full(s->console);
}

static const GraphicHwOps apple_h12p_ops = {
    .gfx_update = fb_gfx_update,
};

static void apple_h12p_realize(DeviceState *dev, Error **errp)
{
    AppleH12PState *s = APPLE_H12P(dev);
    timer_mod(s->timer, qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + 0x33C);

    s->console = graphic_console_init(dev, 0, &apple_h12p_ops, s);
    qemu_console_resize(s->console, s->width, s->height);
}

static const VMStateDescription vmstate_apple_h12p = {
    .name = TYPE_APPLE_H12P,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(width, AppleH12PState),
        VMSTATE_UINT32(height, AppleH12PState),
        VMSTATE_END_OF_LIST()
    },
};

static Property apple_h12p_props[] = {
    DEFINE_PROP_UINT32("width", AppleH12PState, width, 480),
    DEFINE_PROP_UINT32("height", AppleH12PState, height, 680),
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

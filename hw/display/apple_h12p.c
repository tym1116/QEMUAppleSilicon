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

static void iomfb_mbox_update_irq(IOMFBMboxState *mbox)
{
    if (((mbox->inbox_ctl | mbox->outbox_ctl) & IOMFB_MBOX_CTL_ENABLE)) {
        qemu_irq_raise(mbox->irq);
    } else {
        qemu_irq_lower(mbox->irq);
    }
}

static bool iomfb_mbox_handle_write(IOMFBMboxState *mbox, uint64_t base, uint64_t addr, uint64_t data) {
    if (addr < base) { return false; }
    addr -= base;
    bool ret = false;
    switch (addr) {
        case IOMFB_MBOX_INT_FILTER:
            mbox->int_filter = (uint32_t)data;
            ret = true;
            break;
        // ---- INBOX ----
        case IOMFB_MBOX_INBOX + IOMFB_MBOX_CTL:
            mbox->inbox_ctl = (uint32_t)data;
            ret = true;
            break;
        case IOMFB_MBOX_INBOX + IOMFB_MBOX_WRITE:
            mbox->inbox_write = (uint32_t)data;
            ret = true;
            break;
        case IOMFB_MBOX_INBOX + IOMFB_MBOX_READ:
            mbox->inbox_read = (uint32_t)data;
            ret = true;
            break;
        // ---- OUTBOX ----
        case IOMFB_MBOX_OUTBOX + IOMFB_MBOX_CTL:
            mbox->outbox_ctl = (uint32_t)data;
            ret = true;
            break;
        case IOMFB_MBOX_OUTBOX + IOMFB_MBOX_WRITE:
            mbox->outbox_write = (uint32_t)data;
            ret = true;
            break;
        case IOMFB_MBOX_OUTBOX + IOMFB_MBOX_READ:
            mbox->outbox_read = (uint32_t)data;
            ret = true;
            break;
    }
    if (ret) {
        iomfb_mbox_update_irq(mbox);
        info_report("iomfb_mbox_handle_write returned true");
    }
    return ret;
}

static bool iomfb_mbox_handle_read(IOMFBMboxState *mbox, uint64_t base, uint64_t addr, uint64_t *data) {
    if (addr < base) { return false; }
    addr -= base;
    bool ret = false;
    switch (addr) {
        case IOMFB_MBOX_INT_FILTER:
            *data = mbox->int_filter;
            ret = true;
            break;
        // ---- INBOX ----
        case IOMFB_MBOX_INBOX + IOMFB_MBOX_CTL:
            *data = mbox->inbox_ctl;
            ret = true;
            break;
        case IOMFB_MBOX_INBOX + IOMFB_MBOX_WRITE:
            *data = mbox->inbox_write;
            ret = true;
            break;
        case IOMFB_MBOX_INBOX + IOMFB_MBOX_READ:
            *data = mbox->inbox_read;
            ret = true;
            break;
        // ---- OUTBOX ----
        case IOMFB_MBOX_OUTBOX + IOMFB_MBOX_CTL:
            *data = mbox->outbox_ctl;
            ret = true;
            break;
        case IOMFB_MBOX_OUTBOX + IOMFB_MBOX_WRITE:
            *data = mbox->outbox_write;
            ret = true;
            break;
        case IOMFB_MBOX_OUTBOX + IOMFB_MBOX_READ:
            *data = mbox->outbox_read;
            ret = true;
            break;
    }
    if (ret) { info_report("iomfb_mbox_handle_read returned true"); }
    return ret;
}

static void iomfb_mbox_init(IOMFBMboxState *mbox, qemu_irq irq) {
    mbox->int_filter = 0xFFFFFFFF;
    mbox->inbox_ctl = 0xFFFFFFFF;
    mbox->inbox_write = 0x0;
    mbox->inbox_read = 0x0;
    mbox->outbox_ctl = 0xFFFFFFFF;
    mbox->outbox_write = 0x0;
    mbox->outbox_read = 0x0;
    mbox->irq = irq;
}

static void h12p_up_write(void *opaque, hwaddr addr, uint64_t data, unsigned size)
{
    AppleH12PState *s = APPLE_H12P(opaque);
    if (!iomfb_mbox_handle_write(&s->apt_mbox, IOMFB_MBOX_REG_BASE_APT, addr, data)) {
        iomfb_mbox_handle_write(&s->pcc_mbox, IOMFB_MBOX_REG_BASE_PCC, addr, data);
    }
    // info_report("h12p_up_write: 0x" TARGET_FMT_plx " <- 0x" TARGET_FMT_plx, addr, data);
}

static uint64_t h12p_up_read(void *opaque, hwaddr addr, unsigned size)
{
    AppleH12PState *s = APPLE_H12P(opaque);
    uint64_t ret = 0;
    switch (addr) {
        case REG_UPPIPE_VER:
            ret = 0x70045;
            break;
        case REG_GENPIPE0_IDK:
            ret = -1;
            break;
        case REG_UPPIPE_FRAME_SIZE:
            ret = (s->width << 16) | s->height;
            break;
        case REG_GENPIPE0_FRAME_SIZE:
        case REG_UP_CONFIG_FRAME_SIZE:
            ret = ((s->width * 4) << 16) | s->height;
            break;
        default:
            if (!iomfb_mbox_handle_read(&s->apt_mbox, IOMFB_MBOX_REG_BASE_APT, addr, &ret)) {
                iomfb_mbox_handle_read(&s->pcc_mbox, IOMFB_MBOX_REG_BASE_PCC, addr, &ret);
            }
            break;
    }
    // info_report("h12p_up_read: 0x" TARGET_FMT_plx " -> 0x" TARGET_FMT_plx, addr, ret);
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

    qdev_init_gpio_out_named(DEVICE(dev), &s->apt_mbox.irq, "disp0.apt_irq", IOMFB_MBOX_IRQ_APT);
    qdev_init_gpio_out_named(DEVICE(dev), &s->pcc_mbox.irq, "disp0.pcc_irq", IOMFB_MBOX_IRQ_PCC);

    iomfb_mbox_init(&s->apt_mbox, s->irqs[IOMFB_MBOX_IRQ_APT]);
    iomfb_mbox_init(&s->pcc_mbox, s->irqs[IOMFB_MBOX_IRQ_PCC]);

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

static void fb_gfx_update(void *opaque)
{
    // TODO: implement this properly
    AppleH12PState *s = APPLE_H12P(opaque);
}

static void fb_invalidate(void *opaque)
{
    // TODO: implement this properly
    printf("FB invalidate called\n");
}

static const GraphicHwOps apple_h12p_ops = {
    .invalidate = fb_invalidate,
    .gfx_update = fb_gfx_update,
};

static void apple_h12p_realize(DeviceState *dev, Error **errp)
{
    AppleH12PState *s = APPLE_H12P(dev);
    iomfb_mbox_update_irq(&s->apt_mbox);
    iomfb_mbox_update_irq(&s->pcc_mbox);

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

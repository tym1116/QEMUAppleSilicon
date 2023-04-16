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
    AppleH12PState *fb = APPLE_H12P(opaque);
    info_report("H12P/UnifiedPipeline: 0x" TARGET_FMT_plx " <- 0x" TARGET_FMT_plx, addr, data);
    *(uint32_t *)&fb->regs[addr] = (uint32_t)(data);
}

static uint64_t h12p_up_read(void *opaque, hwaddr addr, unsigned size)
{
    AppleH12PState *fb = APPLE_H12P(opaque);
    uint64_t ret = *(uint32_t *)&fb->regs[addr];
    info_report("H12P/UnifiedPipeline: 0x" TARGET_FMT_plx " -> 0x" TARGET_FMT_plx, addr, ret);
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
    // tms->video.v_baseAddr = T8030_DISPLAY_BASE;
    tms->video.v_rowBytes = 512 * 4;
    tms->video.v_width = 512;
    tms->video.v_height = 512;
    tms->video.v_depth = 32 | ((2 - 1) << 16);
    // tms->video.v_display = 1;

    if (xnu_contains_boot_arg(machine->kernel_cmdline, "-s", false)
        || xnu_contains_boot_arg(machine->kernel_cmdline, "-v", false)) {
        tms->video.v_display = 0;
    }

    SysBusDevice *sbd = SYS_BUS_DEVICE(qdev_new(TYPE_APPLE_H12P));
    object_property_set_uint(OBJECT(sbd), "width", 512, &error_fatal);
    object_property_set_uint(OBJECT(sbd), "height", 512, &error_fatal);

    AppleH12PState *h12p = APPLE_H12P(sbd);
    h12p->cnt = 0x1E;
    *(uint32_t *)&h12p->regs[REG_UPPIPE_VER] = 0x70045; // No A0 SoC
    *(uint32_t *)&h12p->regs[REG_GENPIPE0_IDK] = -1;
    *(uint32_t *)&h12p->regs[REG_GENPIPE0_FRAME_SIZE] = (512 << 0x10) | 512;
    DTBNode *armio = find_dtb_node(tms->device_tree, "arm-io");
    assert(armio);
    DTBNode *child = find_dtb_node(armio, "disp0");
    assert(child);
    assert(set_dtb_prop(child, "display-target", 15, "DisplayTarget5"));
    uint8_t dispTimingInfo[] = {0x3C, 0x03, 0x00, 0x00, 0x90, 0x00, 0x00, 0x00, 0x01, 0x00,
                                0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00,
                                0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00,
                                0x00, 0x00};
    assert(set_dtb_prop(child, "display-timing-info", 32, &dispTimingInfo));
    DTBProp *prop = find_dtb_prop(child, "reg");
    assert(prop);
    MemoryRegion *h12p_mem = g_new(MemoryRegion, 1);
    uint64_t *reg = (uint64_t*)prop->value;
    memory_region_init_io(h12p_mem, OBJECT(sbd), &h12p_up_ops, sbd, "up.regs", reg[1]);
    sysbus_init_mmio(sbd, h12p_mem);
    sysbus_mmio_map(sbd, 0, tms->soc_base_pa + reg[0]);
    object_property_add_const_link(OBJECT(sbd), "up.regs", OBJECT(h12p_mem));
    AppleDARTState *dart = APPLE_DART(object_property_get_link(OBJECT(machine),
                      "dart-disp0", &error_fatal));
    assert(dart);

    child = find_dtb_node(armio, "dart-disp0");
    assert(child);
    child = find_dtb_node(child, "mapper-disp0");
    assert(child);
    prop = find_dtb_prop(child, "reg");
    assert(prop);
    h12p->dma_mr = MEMORY_REGION(apple_dart_iommu_mr(dart, *(uint32_t *)prop->value));
    assert(h12p->dma_mr);
    assert(object_property_add_const_link(OBJECT(sbd), "dma_mr", OBJECT(h12p->dma_mr)));
    address_space_init(&h12p->dma_as, h12p->dma_mr, "disp0");

    child = find_dtb_node(tms->device_tree, "product");
    uint64_t data64 = 0x100000027;
    assert(set_dtb_prop(child, "display-corner-radius", 8, &data64));
    uint32_t data = 0x1;
    assert(set_dtb_prop(child, "oled-display", 4, &data));
    assert(set_dtb_prop(child, "graphics-featureset-class", 7, "MTL1,2"));
    assert(set_dtb_prop(child, "graphics-featureset-fallbacks", 15, "MTL1,2:GLES2,0"));
    assert(set_dtb_prop(tms->device_tree, "target-type", 4, "sim")); // TODO: implement PMP

    // MemoryRegion *vram = g_new(MemoryRegion, 1);
    // memory_region_init_ram(vram, OBJECT(fb), "vram", T8030_DISPLAY_SIZE, &error_fatal);
    // memory_region_add_subregion_overlap(tms->sysmem, tms->video.v_baseAddr, vram, 1);
    // object_property_add_const_link(OBJECT(sbd), "vram", OBJECT(vram));
    object_property_add_child(OBJECT(machine), "sbd", OBJECT(sbd));

    sysbus_realize_and_unref(sbd, &error_fatal);
}

static void fb_draw_row(void *opaque, uint8_t *dest, const uint8_t *src,
                        int width, int dest_pitch)
{
    while (width--) {
        /* Load using endian-safe loads */
        uint32_t color = ldl_le_p(src);
        /* Increment source pointer */
        src += dest_pitch;

        /* Blit it to the display output now that it's converted */
        /* FIXME this might not be endian-safe but the rest should be */
        memcpy(dest, &color, sizeof(color));
        /*
         * NOTE: We always assume that pixels are packed end to end so we
         * ignore dest_pitch
         */
        dest += dest_pitch;
    }
}

static void fb_gfx_update(void *opaque)
{
    AppleH12PState *s = APPLE_H12P(opaque);
    DisplaySurface *surface = qemu_console_surface(s->console);

    /* Used as both input to start converting fb memory and output of dirty */
    int first_row = 0;
    /* Output of last row of fb that was updated during conversion */
    int last_row;

    int width = s->width;
    int height = s->height;
    int stride = width * 4; /* Bytes per line is 4*pixels */

    /* TODO: this is the only way to tell if it's not initialized since
     * vram_section isn't a pointer. We should just handle invalidate properly
     */
    // if (s->vram_section.mr == NULL) {
    //     framebuffer_update_memory_section(&s->vram_section, s->vram, 0,
    //                                       height, src_stride);
    // }

    // /*
    //  * Update the display memory that's changed using fb_draw_row to convert
    //  * between the source and destination pixel formats
    //  */
    // framebuffer_update_display(surface, &s->vram_section,
    //                            width, height,
    //                            src_stride, dest_stride, 0, 0,
    //                            fb_draw_row, s, &first_row, &last_row);

    /* If anything changed update that region of the display */
    if (first_row >= 0) {
        /* # of rows that were updated, including row 1 (offset 0) */
        int updated_rows = last_row - first_row + 1;
        dpy_gfx_update(s->console, 0, first_row, width, updated_rows);
    }
}

static void fb_invalidate(void *opaque)
{
    printf("FB invalidate called\n");
}

static const GraphicHwOps apple_h12p_ops = {
        .invalidate = fb_invalidate,
        .gfx_update = fb_gfx_update,
};

static void apple_h12p_realize(DeviceState *dev, Error **errp)
{
    printf("Qemu FB realize\n");
    AppleH12PState *s = APPLE_H12P(dev);

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
        }
};

static Property apple_h12p_props[] = {
        DEFINE_PROP_UINT32("width", AppleH12PState, width, 512),
        DEFINE_PROP_UINT32("height", AppleH12PState, height, 512),
        DEFINE_PROP_END_OF_LIST()
};

static void apple_h12p_class_init(ObjectClass *oc, void *data) {
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

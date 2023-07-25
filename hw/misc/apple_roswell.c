/*
 * Apple Roswell.
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
#include "hw/i2c/apple_i2c.h"
#include "hw/misc/apple_roswell.h"
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "qemu/error-report.h"

static uint8_t apple_roswell_rx(I2CSlave *i2c)
{
    return 0x00;
}

static int apple_roswell_tx(I2CSlave *i2c, uint8_t data)
{
    return 0;
}

static int apple_roswell_event(I2CSlave *i2c, enum i2c_event event)
{
    return 0;
}

void apple_roswell_create(MachineState *machine)
{
    AppleI2CState *i2c = APPLE_I2C(
        object_property_get_link(OBJECT(machine), "i2c3", &error_fatal));
    i2c_slave_create_simple(i2c->bus, TYPE_APPLE_ROSWELL, 0x10);
}

static void apple_roswell_realize(DeviceState *dev, Error **errp)
{
}

static void apple_roswell_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    I2CSlaveClass *c = I2C_SLAVE_CLASS(oc);

    dc->desc = "Apple Roswell";
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);

    dc->realize = apple_roswell_realize;
    c->event = apple_roswell_event;
    c->recv = apple_roswell_rx;
    c->send = apple_roswell_tx;
}

static const TypeInfo apple_roswell_type_info = {
    .name = TYPE_APPLE_ROSWELL,
    .parent = TYPE_I2C_SLAVE,
    .instance_size = sizeof(AppleRoswellState),
    .class_init = apple_roswell_class_init,
};

static void apple_roswell_register_types(void)
{
    type_register_static(&apple_roswell_type_info);
}

type_init(apple_roswell_register_types);

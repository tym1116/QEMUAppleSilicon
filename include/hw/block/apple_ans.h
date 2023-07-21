#ifndef APPLE_ANS_H
#define APPLE_ANS_H

#include "hw/arm/xnu_dtb.h"
#include "hw/sysbus.h"
#include "qemu/queue.h"
#include "qom/object.h"

SysBusDevice *apple_ans_create(DTBNode *node, uint32_t protocol_version);

#endif /* APPLE_ANS_H */

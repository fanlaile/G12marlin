#pragma once

#include <stdint.h>
// #include <stdbool.h>

bool usbh_disk_init(void (*mount_cb)(void), void (*unmount_cb)(void));
bool usbh_disk_is_inserted(void);
bool usbh_disk_read_block(uint32_t block, uint8_t *dst);
bool usbh_disk_write_block(uint32_t block, const uint8_t *src);
void usbh_disk_process(void);

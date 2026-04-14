// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2007-2013
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Jerry Wang <wangflord@allwinnertech.com>
 */

#ifndef __SPRITE_H
#define __SPRITE_H

#include <common.h>

#define SPRITE_LED_OFF 0
#define SPRITE_LED_ON 1

uint sunxi_sprite_generate_checksum(void *buffer, uint length, uint src_sum);
int sunxi_sprite_verify_checksum(void *buffer, uint length, uint src_sum);
int sunxi_sprite_verify_mbr(void *buffer);

int sunxi_sprite_download_boot0(void *buffer, int production_media);
int sunxi_sprite_download_uboot(void *buffer, int production_media, int generate_checksum);
int sunxi_sprite_download_mbr(void *buffer, uint buffer_size);

extern int sunxi_card_sprite_main(int workmode, char *name);

#ifdef CONFIG_SUNXI_SPRITE_LED
int sprite_led_init(void);
int sprite_led_turn(int data);
int sprite_led_exit(int status);
#endif

#endif /* __SPRITE_H */

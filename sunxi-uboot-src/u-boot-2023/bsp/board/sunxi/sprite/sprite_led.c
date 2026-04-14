// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2007-2013
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 */

#include <common.h>
#include <asm/arch/timer.h>
#include <asm/arch/cpu.h>
#include <fdt_support.h>
#include <asm-generic/gpio.h>
#include <sunxi_board.h>
#include <sprite.h>
#include <sys_config.h>

struct timer_list TIMER0;
static int sprite_led_status;
static struct gpio_desc sprite_led_hd;

/*
 * 函数名称：sprite_timer_func
 * 说明    ：定时器回调函数，用于控制LED闪烁
 * 输入    ：p - 定时器参数
 * 输出    ：无
 */
static void sprite_timer_func(void *p)
{
    /* 翻转电平 */
    dm_gpio_set_value(&sprite_led_hd, sprite_led_status);
    sprite_led_status = (~sprite_led_status) & 0x01;

    /* 重新启动定时器 */
    del_timer(&TIMER0);
    add_timer(&TIMER0);
}

/*
 * 函数名称：sprite_led_init
 * 说明    ：初始化LED GPIO和定时器
 * 输入    ：void
 * 输出    ：0 - 成功，非0 - 失败
 */
int sprite_led_init(void)
{
    ofnode node;
    int ret   = 0;
    int delay = 0;

    sprite_led_status = 1;

    /* 获取 DT 节点 */
    node = ofnode_path(FDT_PATH_CARD_BOOT);
    if (!ofnode_valid(node)) {
	pr_err("cannot find node\n");
	return -ENODEV;
    }

    /* 正常工作时，灯闪烁的时间 */
    delay = ofnode_read_s32_default(node, "sprite_work_delay", 500);
    pr_info("try sprite_led_gpio config\n");

    /* 获取 gpio，属性名是 sprite_gpio0 */
    ret = gpio_request_by_name_nodev(node, "sprite_gpio0", 0, &sprite_led_hd, GPIOD_IS_OUT);
    if (ret) {
	pr_err("request gpio for led failed (%d)\n", ret);
	return 1;
    }

    /* 配置定时器 */
    TIMER0.data	    = (unsigned long)&TIMER0;
    TIMER0.expires  = delay;
    TIMER0.function = sprite_timer_func;
    add_timer(&TIMER0);

    pr_info("sprite_led_gpio start\n");

    return 0;
}

/*
 * 函数名称：sprite_led_turn
 * 说明    ：直接控制LED开关
 * 输入    ：data - LED状态（0关闭，1打开）
 * 输出    ：0 - 成功，非0 - 失败
 */
int sprite_led_turn(int data)
{
    ofnode node;
    int ret;

    node = ofnode_path(FDT_PATH_CARD_BOOT);
    gpio_request_by_name_nodev(node, "sprite_gpio0", 0, &sprite_led_hd, GPIOD_IS_OUT);
    if (!dm_gpio_is_valid(&sprite_led_hd)) {
	pr_err("led gpio not valid\n");
	return -ENODEV;
    }

    printf("sprite_led_turn:%d\n", data);
    ret = dm_gpio_set_value(&sprite_led_hd, data);
    if (ret) {
	printf("GPIO set value failed, error code: %d\n", ret);
	return ret;
    } else {
	printf("GPIO set value successfully\n");
    }
    sprite_led_status = data;

    return 0;
}

/*
 * 函数名称：sprite_led_exit
 * 说明    ：停止或调整LED闪烁
 * 输入    ：status - 退出状态，负数表示出错
 * 输出    ：0 - 成功，非0 - 失败
 */
int sprite_led_exit(int status)
{
    int delay = 100;
    ofnode node;

    pr_info("sprite_led_gpio stop\n");
    del_timer(&TIMER0);

    if (status < 0) {
	/* 出错时加快闪烁 */
	node = ofnode_path(FDT_PATH_CARD_BOOT);
	if (ofnode_valid(node)) {
	    delay = ofnode_read_s32_default(node, "sprite_err_delay", 100);
	}
	if (!delay)
	    delay = 100;

	TIMER0.data	= (unsigned long)&TIMER0;
	TIMER0.expires	= delay;
	TIMER0.function = sprite_timer_func;
	add_timer(&TIMER0);
    }

    return 0;
}
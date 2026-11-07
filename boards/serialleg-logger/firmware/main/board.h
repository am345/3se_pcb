#pragma once

#include "driver/gpio.h"

/*
 * 板级引脚，来源：hardware/pcb/Logger.epcb2 中 U1（ESP32-S3-MINI-1-N4R2）焊盘网络，
 * 对应 2026-10-16 丝印与 USB 短线修订版。改板后须重新核对。
 *
 * USB D-/D+ 固定为 GPIO19/20（U1 焊盘 23/24），由 USB 外设直接使用，这里不再定义。
 * UART0 下载口 GPIO43/44 引到 J4，作为调试控制台。
 */

/* 指示灯：GPIO → R26/R27 → LED → GND，高电平点亮 */
#define BOARD_PIN_LED_REC   GPIO_NUM_8   /* D1，记录 */
#define BOARD_PIN_LED_ERR   GPIO_NUM_9   /* D2，错误 */

/* SW2 BOOT 键：R3 上拉，按下为低。上电后复用为“停止/继续记录”键 */
#define BOARD_PIN_KEY       GPIO_NUM_0

/* microSD，4-bit SDMMC，经 GPIO matrix 映射；CMD/D0~D3 板上已有 10 kΩ 上拉 */
#define BOARD_PIN_SD_CLK    GPIO_NUM_14
#define BOARD_PIN_SD_CMD    GPIO_NUM_13
#define BOARD_PIN_SD_D0     GPIO_NUM_15
#define BOARD_PIN_SD_D1     GPIO_NUM_16
#define BOARD_PIN_SD_D2     GPIO_NUM_11
#define BOARD_PIN_SD_D3     GPIO_NUM_12
#define BOARD_PIN_SD_CD_N   GPIO_NUM_17  /* 卡检测，R24 上拉 */

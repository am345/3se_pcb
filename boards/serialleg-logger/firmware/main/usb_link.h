#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"

/*
 * USB 主机 + CDC-ACM：打开 DM-MC02 的虚拟串口，把收到的原始字节非阻塞写入环形缓冲。
 * J2 的 VBUS 不接，依赖主控自供电且关闭 VBUS 感知；日志板不向主控发送任何数据。
 */

typedef struct {
    bool connected;
    uint32_t session;           /* 每次打开设备加一 */
    uint16_t vid;
    uint16_t pid;
    uint32_t rx_bytes;          /* 累计收到字节，32 位回绕 */
    uint32_t drop_bytes;        /* 环形缓冲满而丢弃的字节 */
    uint32_t drop_events;
    uint32_t last_rx_tick;      /* FreeRTOS tick */
} usb_link_status_t;

/* 主机端口寄存器 HPRT 的诊断位：是否检测到设备、端口供电，以及 D+/D- 当前电平 */
typedef struct {
    bool connected;
    bool powered;
    bool dp;
    bool dm;
} usb_link_port_t;

esp_err_t usb_link_start(RingbufHandle_t ring);
void usb_link_get_status(usb_link_status_t *out);
void usb_link_get_port(usb_link_port_t *out);

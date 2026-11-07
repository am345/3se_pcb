#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"

typedef enum {
    LOGGER_NO_CARD,     /* 未插卡 */
    LOGGER_CARD_ERROR,  /* 挂载或写卡失败，定时重试 */
    LOGGER_CARD_FULL,   /* 剩余空间不足，换卡前不再写入 */
    LOGGER_IDLE,        /* 卡就绪，等待 USB 设备或数据 */
    LOGGER_RECORDING,   /* 文件已打开 */
    LOGGER_PAUSED,      /* 按键停止，文件已安全关闭 */
} logger_state_t;

/* 供指示灯显示的状态快照 */
typedef struct {
    logger_state_t state;
    bool usb_connected;
    bool data_recent;   /* 1 s 内收到过 USB 数据 */
    bool file_warning;  /* 当前文件出现缓冲丢弃或 CRC 错误 */
} logger_view_t;

esp_err_t logger_start(RingbufHandle_t ring, size_t ring_size);
void logger_get_view(logger_view_t *out);
/* 按键请求：记录中则关闭文件并暂停，暂停中则恢复 */
void logger_request_toggle(void);

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

/*
 * 使能前的预缓冲：未记录时把收到的原始字节放进 PSRAM 环形区，按到达时间只保留最近 window_ms；
 * 开始记录时先把这段数据写进新文件。数据不按帧边界切，解码时靠帧头与 CRC 重新对齐。
 * 只由记录任务调用，不做线程保护。
 */

/* bytes 为 0 或 PSRAM 不足时预缓冲关闭，其余接口变为空操作 */
esp_err_t preroll_init(size_t bytes, uint32_t window_ms);
void preroll_append(const uint8_t *data, size_t len, TickType_t now);
void preroll_clear(void);
size_t preroll_bytes(void);

typedef esp_err_t (*preroll_sink_t)(const uint8_t *data, size_t len);
/* 从最早到最新依次交给 sink，结束后清空；返回 sink 的第一个错误 */
esp_err_t preroll_flush(TickType_t now, preroll_sink_t sink);

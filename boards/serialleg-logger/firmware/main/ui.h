#pragma once

#include "esp_err.h"

/* 指示灯与按键：D1 记录灯、D2 错误灯、SW2(BOOT) 停止/继续记录 */
esp_err_t ui_start(void);

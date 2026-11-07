#include "ui.h"

#include "board.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "logger.h"

#define UI_TICK_MS          20
/* 按住约 1 s 才算一次按键：下载器 DTR 接在 BOOT(GPIO0) 上，串口工具开关串口时的短脉冲不会误触发 */
#define KEY_HOLD_TICKS      (1000 / UI_TICK_MS)

static const char *TAG = "ui";

/* period_ms 内依次为 on/off 段，长度单位 ms；全 0 表示常灭 */
typedef struct {
    uint16_t period_ms;
    uint16_t on1_ms;
    uint16_t gap_ms;
    uint16_t on2_ms;
} blink_t;

static const blink_t BLINK_OFF       = {0, 0, 0, 0};
static const blink_t BLINK_ON        = {1000, 1000, 0, 0};
static const blink_t BLINK_HEARTBEAT = {1000, 50, 0, 0};
static const blink_t BLINK_DOUBLE    = {2000, 50, 150, 50};
static const blink_t BLINK_2HZ       = {500, 250, 0, 0};
static const blink_t BLINK_5HZ       = {200, 100, 0, 0};
static const blink_t BLINK_1HZ       = {1000, 500, 0, 0};

static bool blink_level(const blink_t *b, uint32_t now_ms)
{
    if (b->period_ms == 0) {
        return false;
    }
    uint32_t t = now_ms % b->period_ms;
    if (t < b->on1_ms) {
        return true;
    }
    t -= b->on1_ms;
    if (t < b->gap_ms) {
        return false;
    }
    t -= b->gap_ms;
    return t < b->on2_ms;
}

static void select_patterns(const logger_view_t *v, const blink_t **rec, const blink_t **err)
{
    *rec = &BLINK_OFF;
    *err = &BLINK_OFF;

    switch (v->state) {
    case LOGGER_NO_CARD:
        *err = &BLINK_ON;
        break;
    case LOGGER_CARD_ERROR:
        *err = &BLINK_5HZ;
        break;
    case LOGGER_CARD_FULL:
        *err = &BLINK_1HZ;
        break;
    case LOGGER_IDLE:
        *rec = v->usb_connected ? &BLINK_2HZ : &BLINK_HEARTBEAT;
        break;
    case LOGGER_RECORDING:
        *rec = v->data_recent ? &BLINK_ON : &BLINK_2HZ;
        if (v->file_warning) {
            *err = &BLINK_HEARTBEAT;
        }
        break;
    case LOGGER_PAUSED:
        *rec = &BLINK_DOUBLE;
        break;
    }
}

static void ui_task(void *arg)
{
    int key_low = 0;
    bool key_armed = false;     /* 上电时若按键仍按住，等松开后再响应 */

    gpio_set_level(BOARD_PIN_LED_REC, 1);
    gpio_set_level(BOARD_PIN_LED_ERR, 1);
    vTaskDelay(pdMS_TO_TICKS(300));

    while (true) {
        uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

        if (gpio_get_level(BOARD_PIN_KEY) == 0) {
            if (key_low < KEY_HOLD_TICKS) {
                key_low++;
                if (key_low == KEY_HOLD_TICKS && key_armed) {
                    logger_request_toggle();
                }
            }
        } else {
            key_low = 0;
            key_armed = true;
        }

        logger_view_t v;
        const blink_t *rec;
        const blink_t *err;
        logger_get_view(&v);
        select_patterns(&v, &rec, &err);
        gpio_set_level(BOARD_PIN_LED_REC, blink_level(rec, now_ms));
        gpio_set_level(BOARD_PIN_LED_ERR, blink_level(err, now_ms));

        vTaskDelay(pdMS_TO_TICKS(UI_TICK_MS));
    }
}

esp_err_t ui_start(void)
{
    const gpio_config_t leds = {
        .pin_bit_mask = (1ULL << BOARD_PIN_LED_REC) | (1ULL << BOARD_PIN_LED_ERR),
        .mode = GPIO_MODE_OUTPUT,
    };
    const gpio_config_t key = {
        .pin_bit_mask = 1ULL << BOARD_PIN_KEY,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };

    esp_err_t err = gpio_config(&leds);
    if (err == ESP_OK) {
        err = gpio_config(&key);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GPIO 初始化失败：%s", esp_err_to_name(err));
        return err;
    }

    if (xTaskCreate(ui_task, "ui", 3072, NULL, 2, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

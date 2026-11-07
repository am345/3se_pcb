#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "logger.h"
#include "preroll.h"
#include "sd_store.h"
#include "sdkconfig.h"
#include "ui.h"
#include "usb_link.h"

#define FALLBACK_RING_SIZE (64 * 1024)

static const char *TAG = "main";

static StaticRingbuffer_t s_ring_struct;

/* USB 接收与写卡之间的环形缓冲：优先放 PSRAM，PSRAM 不可用时退回内部 RAM */
static RingbufHandle_t create_ring(size_t *size_out)
{
    size_t size = (size_t)CONFIG_LOGGER_RING_KB * 1024;
    uint8_t *storage = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (storage == NULL) {
        size = FALLBACK_RING_SIZE;
        storage = heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        ESP_LOGW(TAG, "PSRAM 不可用，环形缓冲退回内部 RAM %u KB", (unsigned)(size / 1024));
    }
    if (storage == NULL) {
        return NULL;
    }

    *size_out = size;
    return xRingbufferCreateStatic(size, RINGBUF_TYPE_BYTEBUF, storage, &s_ring_struct);
}

void app_main(void)
{
    const esp_app_desc_t *app = esp_app_get_description();
    ESP_LOGI(TAG, "SerialLeg Logger %s（ESP-IDF %s）", app->version, app->idf_ver);

    size_t ring_size = 0;
    RingbufHandle_t ring = create_ring(&ring_size);
    if (ring == NULL) {
        ESP_LOGE(TAG, "环形缓冲分配失败");
        abort();
    }
    ESP_LOGI(TAG, "环形缓冲 %u KB，PSRAM 剩余 %u KB", (unsigned)(ring_size / 1024),
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));

#if CONFIG_LOGGER_RECORD_ENABLED_ONLY
    /* 预缓冲分配失败时只记录使能之后的数据，不影响启动 */
    (void)preroll_init((size_t)CONFIG_LOGGER_PREROLL_KB * 1024, CONFIG_LOGGER_PREROLL_MS);
#endif
    ESP_ERROR_CHECK(sd_store_init());
    ESP_ERROR_CHECK(ui_start());
    ESP_ERROR_CHECK(logger_start(ring, ring_size));
    ESP_ERROR_CHECK(usb_link_start(ring));
}

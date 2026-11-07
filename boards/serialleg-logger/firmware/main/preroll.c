#include "preroll.h"

#include <string.h>
#include "esp_heap_caps.h"
#include "esp_log.h"

#define PREROLL_CHUNKS 2048U    /* 每次 USB 接收块一条记录；3 s 内约 1000 块 */

static const char *TAG = "preroll";

typedef struct {
    TickType_t tick;
    uint32_t len;
} chunk_t;

static uint8_t *s_buf;
static size_t s_cap;
static TickType_t s_window;
static size_t s_wr;             /* 下一次写入的偏移 */
static size_t s_rd;             /* 最早数据的偏移 */
static size_t s_used;
static chunk_t s_chunks[PREROLL_CHUNKS];
static uint32_t s_first;        /* 最早一块的下标 */
static uint32_t s_count;

esp_err_t preroll_init(size_t bytes, uint32_t window_ms)
{
    s_window = pdMS_TO_TICKS(window_ms);
    if (bytes == 0) {
        return ESP_OK;
    }
    s_buf = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (s_buf == NULL) {
        ESP_LOGW(TAG, "PSRAM 不足，预缓冲关闭");
        return ESP_ERR_NO_MEM;
    }
    s_cap = bytes;
    ESP_LOGI(TAG, "预缓冲 %u KB，保留 %lu ms", (unsigned)(bytes / 1024), (unsigned long)window_ms);
    return ESP_OK;
}

static void drop_oldest(void)
{
    size_t len = s_chunks[s_first].len;

    s_rd = (s_rd + len) % s_cap;
    s_used -= len;
    s_first = (s_first + 1U) % PREROLL_CHUNKS;
    s_count--;
}

static void drop_expired(TickType_t now)
{
    while (s_count > 0 && (now - s_chunks[s_first].tick) > s_window) {
        drop_oldest();
    }
}

void preroll_append(const uint8_t *data, size_t len, TickType_t now)
{
    if (s_cap == 0 || len == 0) {
        return;
    }
    if (len > s_cap) {
        data += len - s_cap;
        len = s_cap;
    }
    drop_expired(now);
    while (s_count > 0 && (s_used + len > s_cap || s_count == PREROLL_CHUNKS)) {
        drop_oldest();
    }

    size_t first = s_cap - s_wr;
    if (first >= len) {
        memcpy(&s_buf[s_wr], data, len);
    } else {
        memcpy(&s_buf[s_wr], data, first);
        memcpy(s_buf, data + first, len - first);
    }
    s_wr = (s_wr + len) % s_cap;
    s_used += len;
    s_chunks[(s_first + s_count) % PREROLL_CHUNKS] = (chunk_t) {.tick = now, .len = (uint32_t)len};
    s_count++;
}

void preroll_clear(void)
{
    s_wr = s_rd = s_used = 0;
    s_first = s_count = 0;
}

size_t preroll_bytes(void)
{
    return s_used;
}

esp_err_t preroll_flush(TickType_t now, preroll_sink_t sink)
{
    esp_err_t err = ESP_OK;

    drop_expired(now);
    while (s_count > 0 && err == ESP_OK) {
        size_t len = s_chunks[s_first].len;
        size_t first = s_cap - s_rd;
        if (first >= len) {
            err = sink(&s_buf[s_rd], len);
        } else {
            err = sink(&s_buf[s_rd], first);
            if (err == ESP_OK) {
                err = sink(s_buf, len - first);
            }
        }
        drop_oldest();
    }
    preroll_clear();
    return err;
}

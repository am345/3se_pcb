#include "logger.h"

#include <inttypes.h>
#include <stdatomic.h>
#include <stdio.h>

#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "link_frame.h"
#include "preroll.h"
#include "sd_store.h"
#include "sdkconfig.h"
#include "usb_link.h"

#define LOOP_WAIT_MS        50
#define RX_CHUNK_MAX        4096
#define MOUNT_RETRY_MS      2000
#define DATA_RECENT_MS      1000
#define MIN_FREE_BYTES      (8ULL * 1024 * 1024)
#define CD_DEBOUNCE_COUNT   3

#if CONFIG_LOGGER_RECORD_ENABLED_ONLY
#define RECORD_GATED        true
#else
#define RECORD_GATED        false
#endif

static const char *TAG = "logger";

typedef struct {
    bool open;
    uint32_t index;
    uint32_t part;
    uint32_t session;
    int64_t open_us;
    uint32_t drop_bytes_at_open;
    uint32_t drop_events_at_open;
    size_t ring_peak;
    uint32_t mc_tick_first;
    bool has_mc_tick;
} file_ctx_t;

static RingbufHandle_t s_ring;
static size_t s_ring_size;
static atomic_bool s_toggle_req;

static portMUX_TYPE s_view_lock = portMUX_INITIALIZER_UNLOCKED;
static logger_view_t s_view;

static logger_state_t s_state = LOGGER_NO_CARD;
static bool s_paused;
static file_ctx_t s_file;
static link_parser_t s_parser;
static usb_link_status_t s_usb;
static uint64_t s_discard_bytes;    /* 无卡、卡错误或暂停时丢弃的字节 */
static TickType_t s_retry_at;
static bool s_cd_last;
static int s_cd_stable;
static TickType_t s_stop_at;        /* 失能后延后关闭的时刻；0 表示未在倒计时 */

void logger_get_view(logger_view_t *out)
{
    portENTER_CRITICAL(&s_view_lock);
    *out = s_view;
    portEXIT_CRITICAL(&s_view_lock);
}

void logger_request_toggle(void)
{
    atomic_store(&s_toggle_req, true);
}

static uint32_t uptime_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static const char *state_name(logger_state_t st)
{
    switch (st) {
    case LOGGER_NO_CARD:    return "NO_CARD";
    case LOGGER_CARD_ERROR: return "CARD_ERR";
    case LOGGER_CARD_FULL:  return "CARD_FULL";
    case LOGGER_IDLE:       return "IDLE";
    case LOGGER_RECORDING:  return "REC";
    case LOGGER_PAUSED:     return "PAUSED";
    }
    return "?";
}

static bool file_warning(void)
{
    return s_file.open &&
           (s_usb.drop_bytes != s_file.drop_bytes_at_open || s_parser.crc_errors > 0);
}

static void update_view(void)
{
    logger_view_t v = {
        .state = s_state,
        .usb_connected = s_usb.connected,
        .data_recent = s_usb.rx_bytes > 0 &&
                       (xTaskGetTickCount() - s_usb.last_rx_tick) < pdMS_TO_TICKS(DATA_RECENT_MS),
        .file_warning = file_warning(),
    };
    portENTER_CRITICAL(&s_view_lock);
    s_view = v;
    portEXIT_CRITICAL(&s_view_lock);
}

static void enter_card_error(void)
{
    sd_store_unmount();
    s_file.open = false;
    s_state = LOGGER_CARD_ERROR;
    s_retry_at = xTaskGetTickCount() + pdMS_TO_TICKS(MOUNT_RETRY_MS);
}

static esp_err_t open_file(uint32_t part)
{
    uint64_t total, free_bytes;

    sd_store_space(&total, &free_bytes);
    if (free_bytes < MIN_FREE_BYTES) {
        ESP_LOGE(TAG, "剩余空间 %llu KB，不再写入", free_bytes >> 10);
        s_state = LOGGER_CARD_FULL;
        return ESP_ERR_NO_MEM;
    }

    uint32_t index;
    esp_err_t err = sd_store_open(&index);
    if (err != ESP_OK) {
        return err;
    }

    s_file = (file_ctx_t) {
        .open = true,
        .index = index,
        .part = part,
        .session = s_usb.session,
        .open_us = esp_timer_get_time(),
        .drop_bytes_at_open = s_usb.drop_bytes,
        .drop_events_at_open = s_usb.drop_events,
    };
    link_parser_reset_counts(&s_parser);
    s_stop_at = 0;

    const esp_app_desc_t *app = esp_app_get_description();
    char meta[640];
    snprintf(meta, sizeof(meta),
             "# SerialLeg Logger 元数据；BIN 为 USB CDC 原始字节流\n"
             "firmware: %s\n"
             "idf: %s\n"
             "file: LOG%05" PRIu32 ".BIN\n"
             "part: %" PRIu32 "\n"
             "usb_session: %" PRIu32 "\n"
             "usb_device: %04X:%04X\n"
             "open_uptime_ms: %" PRIu32 "\n"
             "sync_period_ms: %d\n"
             "record_mode: %s\n"
             "preroll_ms: %d\n"
             "postroll_ms: %d\n"
             "preroll_bytes: %u\n"
             "open_mc_tick_ms: %" PRIu32 "\n",
             app->version, app->idf_ver, index, part, s_usb.session,
             s_usb.vid, s_usb.pid, uptime_ms(), CONFIG_LOGGER_SYNC_MS,
             RECORD_GATED ? "enabled_only" : "always", CONFIG_LOGGER_PREROLL_MS,
             CONFIG_LOGGER_POSTROLL_MS, (unsigned)preroll_bytes(), s_parser.last_tick_ms);
    if (sd_store_append_meta(meta) != ESP_OK) {
        ESP_LOGW(TAG, "元数据写入失败");
    }

    s_state = LOGGER_RECORDING;
    return ESP_OK;
}

static void close_file(const char *reason)
{
    if (!s_file.open) {
        return;
    }

    esp_err_t err = sd_store_close();
    s_file.open = false;

    uint32_t duration_ms = (uint32_t)((esp_timer_get_time() - s_file.open_us) / 1000);
    const uint32_t *t = s_parser.type_frames;
    char meta[896];
    snprintf(meta, sizeof(meta),
             "# 以下帧统计自文件打开起，不含预缓冲内容\n"
             "close_reason: %s\n"
             "close_ok: %s\n"
             "close_uptime_ms: %" PRIu32 "\n"
             "duration_ms: %" PRIu32 "\n"
             "bytes: %llu\n"
             "frames_ok: %" PRIu32 "\n"
             "state_frames: %" PRIu32 "\n"
             "motor_frames: %" PRIu32 "\n"
             "event_frames: %" PRIu32 "\n"
             "health_frames: %" PRIu32 "\n"
             "policy_frames: %" PRIu32 "\n"
             "control_frames: %" PRIu32 "\n"
             "other_frames: %" PRIu32 "\n"
             "crc_errors: %" PRIu32 "\n"
             "length_errors: %" PRIu32 "\n"
             "seq_lost: %" PRIu32 "\n"
             "mc_tick_first_ms: %" PRIu32 "\n"
             "mc_tick_last_ms: %" PRIu32 "\n"
             "logger_drop_bytes: %" PRIu32 "\n"
             "logger_drop_events: %" PRIu32 "\n"
             "ring_peak_bytes: %u\n"
             "ring_size_bytes: %u\n"
             "write_max_ms: %" PRIu32 "\n",
             reason, err == ESP_OK ? "yes" : "no", uptime_ms(), duration_ms,
             sd_store_file_bytes(), s_parser.frames_ok, t[LINK_SLOT_STATE], t[LINK_SLOT_MOTOR],
             t[LINK_SLOT_EVENT], t[LINK_SLOT_HEALTH], t[LINK_SLOT_POLICY], t[LINK_SLOT_CONTROL],
             t[LINK_SLOT_OTHER], s_parser.crc_errors, s_parser.length_errors, s_parser.seq_lost,
             s_file.mc_tick_first, s_parser.last_tick_ms,
             s_usb.drop_bytes - s_file.drop_bytes_at_open,
             s_usb.drop_events - s_file.drop_events_at_open,
             (unsigned)s_file.ring_peak, (unsigned)s_ring_size, sd_store_write_max_ms());
    sd_store_append_meta(meta);

    if (err != ESP_OK) {
        enter_card_error();
    } else {
        s_state = s_paused ? LOGGER_PAUSED : LOGGER_IDLE;
    }
}

static bool can_record(void)
{
    return sd_store_mounted() && !s_paused && s_usb.connected &&
           (s_state == LOGGER_IDLE || s_state == LOGGER_RECORDING);
}

static void write_failed(void)
{
    ESP_LOGE(TAG, "写卡失败，卸载后重试");
    sd_store_abort();
    s_file.open = false;
    sd_store_append_meta("close_reason: write_error\n");
    enter_card_error();
}

static void consume(const uint8_t *data, size_t len)
{
    /* 先解析：使能状态来自数据里的 STATE 帧 */
    link_parser_feed(&s_parser, data, len);

    if (!s_file.open) {
        /* 刚连上时数据可能先于本轮状态快照到达，重新取一次再判断，避免丢掉开头几帧 */
        usb_link_get_status(&s_usb);
        if (!can_record()) {
            s_discard_bytes += len;
            preroll_clear();
            return;
        }
        if (RECORD_GATED && !s_parser.enabled) {
            preroll_append(data, len, xTaskGetTickCount());
            return;
        }
        esp_err_t err = open_file(1);
        if (err != ESP_OK) {
            s_discard_bytes += len;
            if (err != ESP_ERR_NO_MEM) {
                /* 卡满已在 open_file 中转入 CARD_FULL；其他失败按卡错误卸载重试 */
                enter_card_error();
            }
            return;
        }
        ESP_LOGI(TAG, "使能，开始记录（预缓冲 %u 字节）", (unsigned)preroll_bytes());
        if (preroll_flush(xTaskGetTickCount(), sd_store_write) != ESP_OK) {
            write_failed();
            return;
        }
    }

    if (!s_file.has_mc_tick && s_parser.state_frames > 0) {
        s_file.has_mc_tick = true;
        s_file.mc_tick_first = s_parser.last_tick_ms;
    }

    if (sd_store_write(data, len) != ESP_OK) {
        write_failed();
    }
}

/* 只在使能时记录：失能后再记 POSTROLL 时间才关文件，期间重新使能则续写同一文件 */
static void handle_enable_gate(TickType_t now)
{
    if (!RECORD_GATED || !s_file.open) {
        s_stop_at = 0;
        return;
    }
    if (s_parser.enabled) {
        s_stop_at = 0;
    } else if (s_stop_at == 0) {
        s_stop_at = now + pdMS_TO_TICKS(CONFIG_LOGGER_POSTROLL_MS);
        if (s_stop_at == 0) {
            s_stop_at = 1;
        }
    } else if ((int32_t)(now - s_stop_at) >= 0) {
        ESP_LOGI(TAG, "失能，停止记录");
        close_file("disabled");
        s_stop_at = 0;
    }
}

/* 把环形缓冲中已有的数据全部写入当前文件，用于会话结束前 */
static void drain_ring(void)
{
    size_t n;
    uint8_t *item;

    while ((item = xRingbufferReceiveUpTo(s_ring, &n, 0, RX_CHUNK_MAX)) != NULL) {
        consume(item, n);
        vRingbufferReturnItem(s_ring, item);
    }
}

static void handle_card(TickType_t now)
{
    bool present = sd_store_card_present();

    /* 连续几次读到相同电平才认定插拔，避免卡座弹片抖动 */
    if (present == s_cd_last) {
        if (s_cd_stable < CD_DEBOUNCE_COUNT) {
            s_cd_stable++;
        }
    } else {
        s_cd_last = present;
        s_cd_stable = 0;
    }
    if (s_cd_stable < CD_DEBOUNCE_COUNT) {
        return;
    }

    if (!present) {
        if (s_state != LOGGER_NO_CARD) {
            ESP_LOGW(TAG, "卡已拔出");
            if (s_file.open) {
                sd_store_abort();
                s_file.open = false;
            }
            sd_store_unmount();
            s_state = LOGGER_NO_CARD;
        }
        return;
    }

    if (sd_store_mounted() || s_state == LOGGER_CARD_FULL) {
        return;
    }
    if (s_state == LOGGER_CARD_ERROR && (int32_t)(now - s_retry_at) < 0) {
        return;
    }

    if (sd_store_mount() == ESP_OK) {
        s_state = s_paused ? LOGGER_PAUSED : LOGGER_IDLE;
    } else {
        s_state = LOGGER_CARD_ERROR;
        s_retry_at = now + pdMS_TO_TICKS(MOUNT_RETRY_MS);
    }
}

static void handle_toggle(void)
{
    if (!atomic_exchange(&s_toggle_req, false)) {
        return;
    }

    s_paused = !s_paused;
    if (s_paused) {
        ESP_LOGI(TAG, "按键停止记录");
        drain_ring();
        close_file("key_stop");
        if (s_state == LOGGER_IDLE) {
            s_state = LOGGER_PAUSED;
        }
    } else {
        ESP_LOGI(TAG, "按键恢复记录");
        if (s_state == LOGGER_PAUSED) {
            s_state = LOGGER_IDLE;
        }
    }
}

static void print_stats(void)
{
    usb_link_port_t port;

    const uint32_t *t = s_parser.type_frames;

    usb_link_get_port(&port);
    ESP_LOGI(TAG, "%s usb=%d en=%d port=c%d p%d dp%d dm%d rx=%" PRIu32 " file=%s%05" PRIu32 " bytes=%llu frames=%" PRIu32
             " st/mo/ev/he/po/ct/ot=%" PRIu32 "/%" PRIu32 "/%" PRIu32 "/%" PRIu32 "/%" PRIu32 "/%" PRIu32 "/%" PRIu32
             " crc=%" PRIu32 " lost=%" PRIu32 " drop=%" PRIu32 " discard=%llu pre=%u ring_peak=%u wmax=%" PRIu32 "ms",
             state_name(s_state), s_usb.connected, s_parser.enabled, port.connected, port.powered, port.dp, port.dm,
             s_usb.rx_bytes, s_file.open ? "LOG" : "-", s_file.open ? s_file.index : 0,
             s_file.open ? sd_store_file_bytes() : 0, s_parser.frames_ok,
             t[LINK_SLOT_STATE], t[LINK_SLOT_MOTOR], t[LINK_SLOT_EVENT], t[LINK_SLOT_HEALTH],
             t[LINK_SLOT_POLICY], t[LINK_SLOT_CONTROL], t[LINK_SLOT_OTHER], s_parser.crc_errors,
             s_parser.seq_lost, s_usb.drop_bytes, s_discard_bytes, (unsigned)preroll_bytes(),
             (unsigned)s_file.ring_peak, s_file.open ? sd_store_write_max_ms() : 0);
}

static void logger_task(void *arg)
{
    const uint64_t max_bytes = (uint64_t)CONFIG_LOGGER_FILE_MAX_MB * 1024 * 1024;
    TickType_t next_sync = 0;
    TickType_t next_stats = 0;

    /* 上电时直接按当前卡检测电平处理，不等去抖 */
    s_cd_last = sd_store_card_present();
    s_cd_stable = CD_DEBOUNCE_COUNT;
    ESP_LOGI(TAG, "卡检测：%s", s_cd_last ? "有卡" : "无卡");

    while (true) {
        TickType_t now = xTaskGetTickCount();

        usb_link_get_status(&s_usb);
        handle_card(now);
        handle_toggle();

        /* USB 断开或换了新会话：先把残留数据写完再关文件；预缓冲里是旧会话的数据，一并丢掉 */
        if (s_file.open && (!s_usb.connected || s_usb.session != s_file.session)) {
            drain_ring();
            close_file("usb_disconnect");
        }
        if (!s_usb.connected) {
            preroll_clear();
        }

        size_t n;
        uint8_t *item = xRingbufferReceiveUpTo(s_ring, &n, pdMS_TO_TICKS(LOOP_WAIT_MS), RX_CHUNK_MAX);
        if (item != NULL) {
            consume(item, n);
            vRingbufferReturnItem(s_ring, item);
        }
        handle_enable_gate(xTaskGetTickCount());

        if (s_file.open) {
            size_t used = s_ring_size - xRingbufferGetCurFreeSize(s_ring);
            if (used > s_file.ring_peak) {
                s_file.ring_peak = used;
            }
        }

        now = xTaskGetTickCount();
        if (s_file.open && (int32_t)(now - next_sync) >= 0) {
            next_sync = now + pdMS_TO_TICKS(CONFIG_LOGGER_SYNC_MS);
            if (sd_store_flush(true) != ESP_OK) {
                ESP_LOGE(TAG, "同步失败，卸载后重试");
                sd_store_abort();
                s_file.open = false;
                enter_card_error();
            } else if (sd_store_file_bytes() >= max_bytes) {
                uint32_t part = s_file.part + 1;
                close_file("rotate");
                if (s_state == LOGGER_IDLE && open_file(part) != ESP_OK) {
                    ESP_LOGE(TAG, "轮转后打开新文件失败");
                }
            }
        }
        if ((int32_t)(now - next_stats) >= 0) {
            next_stats = now + pdMS_TO_TICKS(CONFIG_LOGGER_STATS_MS);
            print_stats();
        }

        update_view();
    }
}

esp_err_t logger_start(RingbufHandle_t ring, size_t ring_size)
{
    s_ring = ring;
    s_ring_size = ring_size;
    if (xTaskCreatePinnedToCore(logger_task, "logger", 8192, NULL, 5, NULL, 1) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

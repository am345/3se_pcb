#include "sd_store.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "board.h"
#include "driver/sdmmc_host.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "sdkconfig.h"

#define WRITE_BUF_SIZE  (32 * 1024)
#define MAX_FILE_INDEX  99999U

static const char *TAG = "sd";

static sdmmc_card_t *s_card;
static uint8_t *s_buf;
static size_t s_buf_len;
static int s_fd = -1;
static uint32_t s_index;
static uint32_t s_next_index = 1;
static uint64_t s_file_bytes;
static uint32_t s_write_max_ms;

esp_err_t sd_store_init(void)
{
    /* 写卡缓冲放内部 DMA 内存，整扇区写入时 SDMMC 可直接 DMA，不经逐扇区拷贝 */
    s_buf = heap_caps_malloc(WRITE_BUF_SIZE, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (s_buf == NULL) {
        return ESP_ERR_NO_MEM;
    }

    const gpio_config_t cd = {
        .pin_bit_mask = 1ULL << BOARD_PIN_SD_CD_N,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    return gpio_config(&cd);
}

bool sd_store_card_present(void)
{
#if CONFIG_LOGGER_SD_CD_ENABLE
    int level = gpio_get_level(BOARD_PIN_SD_CD_N);
#if CONFIG_LOGGER_SD_CD_ACTIVE_LOW
    return level == 0;
#else
    return level != 0;
#endif
#else
    return true;
#endif
}

bool sd_store_mounted(void)
{
    return s_card != NULL;
}

static void scan_next_index(void)
{
    uint32_t max_index = 0;
    DIR *dir = opendir(SD_MOUNT_POINT);

    if (dir != NULL) {
        struct dirent *e;
        while ((e = readdir(dir)) != NULL) {
            unsigned n;
            char ext[4];
            /* FAT 8.3 短文件名，目录读出为大写；BIN 或 TXT 任一存在都算占用 */
            if (sscanf(e->d_name, "LOG%5u.%3s", &n, ext) == 2 &&
                (strcmp(ext, "BIN") == 0 || strcmp(ext, "TXT") == 0) && n > max_index) {
                max_index = n;
            }
        }
        closedir(dir);
    }
    s_next_index = max_index + 1U;
}

esp_err_t sd_store_mount(void)
{
    if (s_card != NULL) {
        return ESP_OK;
    }

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = CONFIG_LOGGER_SD_FREQ_KHZ;

    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 4;
    slot.clk = BOARD_PIN_SD_CLK;
    slot.cmd = BOARD_PIN_SD_CMD;
    slot.d0 = BOARD_PIN_SD_D0;
    slot.d1 = BOARD_PIN_SD_D1;
    slot.d2 = BOARD_PIN_SD_D2;
    slot.d3 = BOARD_PIN_SD_D3;
    /* 卡检测由记录任务自行轮询，驱动侧不接 */
    slot.cd = SDMMC_SLOT_NO_CD;
    slot.wp = SDMMC_SLOT_NO_WP;

    const esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,
        .max_files = 4,
        .allocation_unit_size = 32 * 1024,
    };

    esp_err_t err = esp_vfs_fat_sdmmc_mount(SD_MOUNT_POINT, &host, &slot, &mount_cfg, &s_card);
    if (err != ESP_OK) {
        s_card = NULL;
        ESP_LOGE(TAG, "挂载失败：%s", esp_err_to_name(err));
        return err;
    }

    sdmmc_card_print_info(stdout, s_card);
    scan_next_index();

    uint64_t total = 0, free_bytes = 0;
    sd_store_space(&total, &free_bytes);
    ESP_LOGI(TAG, "已挂载，容量 %llu MB，剩余 %llu MB，下一个文件 LOG%05lu.BIN",
             total >> 20, free_bytes >> 20, (unsigned long)s_next_index);
    return ESP_OK;
}

void sd_store_unmount(void)
{
    if (s_fd >= 0) {
        sd_store_abort();
    }
    if (s_card != NULL) {
        esp_vfs_fat_sdcard_unmount(SD_MOUNT_POINT, s_card);
        s_card = NULL;
        ESP_LOGI(TAG, "已卸载");
    }
}

void sd_store_space(uint64_t *total_bytes, uint64_t *free_bytes)
{
    if (esp_vfs_fat_info(SD_MOUNT_POINT, total_bytes, free_bytes) != ESP_OK) {
        *total_bytes = 0;
        *free_bytes = 0;
    }
}

static void make_path(char *path, size_t size, uint32_t index, const char *ext)
{
    snprintf(path, size, SD_MOUNT_POINT "/LOG%05lu.%s", (unsigned long)index, ext);
}

esp_err_t sd_store_open(uint32_t *index_out)
{
    char path[32];

    if (s_card == NULL || s_fd >= 0) {
        return ESP_ERR_INVALID_STATE;
    }

    /* 序号被占用（例如卡上残留同名文件）时继续往后找 */
    while (s_next_index <= MAX_FILE_INDEX) {
        make_path(path, sizeof(path), s_next_index, "BIN");
        s_fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0666);
        if (s_fd >= 0) {
            break;
        }
        if (errno != EEXIST) {
            ESP_LOGE(TAG, "创建 %s 失败：errno %d", path, errno);
            return ESP_FAIL;
        }
        s_next_index++;
    }
    if (s_fd < 0) {
        ESP_LOGE(TAG, "文件序号已用尽");
        return ESP_ERR_NOT_FOUND;
    }

    s_index = s_next_index++;
    s_buf_len = 0;
    s_file_bytes = 0;
    s_write_max_ms = 0;
    *index_out = s_index;
    ESP_LOGI(TAG, "开始写入 %s", path);
    return ESP_OK;
}

bool sd_store_is_open(void)
{
    return s_fd >= 0;
}

static void note_duration(int64_t t0)
{
    uint32_t ms = (uint32_t)((esp_timer_get_time() - t0) / 1000);
    if (ms > s_write_max_ms) {
        s_write_max_ms = ms;
    }
}

static esp_err_t write_out(void)
{
    size_t done = 0;
    int64_t t0 = esp_timer_get_time();

    while (done < s_buf_len) {
        ssize_t n = write(s_fd, s_buf + done, s_buf_len - done);
        if (n <= 0) {
            ESP_LOGE(TAG, "写卡失败：errno %d", errno);
            return ESP_FAIL;
        }
        done += (size_t)n;
    }
    note_duration(t0);
    s_file_bytes += s_buf_len;
    s_buf_len = 0;
    return ESP_OK;
}

esp_err_t sd_store_write(const uint8_t *data, size_t len)
{
    if (s_fd < 0) {
        return ESP_ERR_INVALID_STATE;
    }

    while (len > 0) {
        size_t n = WRITE_BUF_SIZE - s_buf_len;
        if (n > len) {
            n = len;
        }
        memcpy(s_buf + s_buf_len, data, n);
        s_buf_len += n;
        data += n;
        len -= n;

        if (s_buf_len == WRITE_BUF_SIZE) {
            esp_err_t err = write_out();
            if (err != ESP_OK) {
                return err;
            }
        }
    }
    return ESP_OK;
}

esp_err_t sd_store_flush(bool sync)
{
    if (s_fd < 0) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_buf_len > 0) {
        esp_err_t err = write_out();
        if (err != ESP_OK) {
            return err;
        }
    }
    if (sync) {
        int64_t t0 = esp_timer_get_time();
        if (fsync(s_fd) != 0) {
            ESP_LOGE(TAG, "fsync 失败：errno %d", errno);
            return ESP_FAIL;
        }
        note_duration(t0);
    }
    return ESP_OK;
}

esp_err_t sd_store_close(void)
{
    if (s_fd < 0) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = sd_store_flush(false);
    if (close(s_fd) != 0 && err == ESP_OK) {
        err = ESP_FAIL;
    }
    s_fd = -1;
    ESP_LOGI(TAG, "关闭 LOG%05lu.BIN，%llu 字节", (unsigned long)s_index, s_file_bytes);
    return err;
}

void sd_store_abort(void)
{
    if (s_fd >= 0) {
        close(s_fd);
        s_fd = -1;
    }
    s_buf_len = 0;
}

esp_err_t sd_store_append_meta(const char *text)
{
    char path[32];

    if (s_card == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    make_path(path, sizeof(path), s_index, "TXT");

    FILE *f = fopen(path, "a");
    if (f == NULL) {
        ESP_LOGE(TAG, "无法写入 %s", path);
        return ESP_FAIL;
    }
    size_t len = strlen(text);
    esp_err_t err = (fwrite(text, 1, len, f) == len) ? ESP_OK : ESP_FAIL;
    if (fclose(f) != 0) {
        err = ESP_FAIL;
    }
    return err;
}

uint64_t sd_store_file_bytes(void)
{
    return s_file_bytes;
}

uint32_t sd_store_write_max_ms(void)
{
    return s_write_max_ms;
}

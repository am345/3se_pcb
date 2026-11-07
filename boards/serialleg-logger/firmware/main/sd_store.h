#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

/*
 * microSD 存储：挂载 FAT、按序号创建 LOGnnnnn.BIN（原始字节）与同名 .TXT（元数据），
 * 内部 DMA 缓冲聚合后再写卡。只由记录任务调用，不做线程保护。
 */

#define SD_MOUNT_POINT "/sd"

esp_err_t sd_store_init(void);
bool sd_store_card_present(void);

esp_err_t sd_store_mount(void);
void sd_store_unmount(void);
bool sd_store_mounted(void);
void sd_store_space(uint64_t *total_bytes, uint64_t *free_bytes);

/* 创建下一个文件，*index_out 返回文件序号 */
esp_err_t sd_store_open(uint32_t *index_out);
bool sd_store_is_open(void);
esp_err_t sd_store_write(const uint8_t *data, size_t len);
/* 把缓冲写入文件；sync 为 true 时同时 fsync，更新目录项中的文件长度 */
esp_err_t sd_store_flush(bool sync);
esp_err_t sd_store_close(void);
/* 已关闭写入出错的文件句柄，不再刷写缓冲 */
void sd_store_abort(void);

/* 追加文本到当前序号的 .TXT 元数据文件 */
esp_err_t sd_store_append_meta(const char *text);

/* 当前文件已交给文件系统的字节数（不含缓冲中未写出的部分） */
uint64_t sd_store_file_bytes(void);
/* 当前文件单次 write/fsync 的最长耗时，打开新文件时清零 */
uint32_t sd_store_write_max_ms(void);

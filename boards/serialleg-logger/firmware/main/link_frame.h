#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * DM-MC02 主控 logger_link 帧的流式统计解析。
 * 帧格式与 serialleg2026 的 Custom/app/telemetry/task_logger_link.h 一致（2026-10-27 前名为 task_nx_comm.h，格式未变）：
 *   A5 5A | type | version | payload_len(u16 LE) | frame_seq(u16 LE) | payload | crc16(LE)
 * CRC 为 CRC-16/MCRF4XX（反射多项式 0x8408，初值 0xFFFF，无末异或），覆盖 CRC 之前全部字节。
 *
 * 日志板始终按原始字节流写卡，这里只做统计，并从 STATE 帧读出主控输出使能，用于决定何时记录。
 */

#define LINK_SOF0             0xA5U
#define LINK_SOF1             0x5AU
#define LINK_HEADER_SIZE      8U
#define LINK_CRC_SIZE         2U
#define LINK_MAX_PAYLOAD      192U
#define LINK_FRAME_MAX_SIZE   (LINK_HEADER_SIZE + LINK_MAX_PAYLOAD + LINK_CRC_SIZE)
#define LINK_MSG_STATE        0x01U
#define LINK_MSG_MOTOR        0x10U
#define LINK_MSG_EVENT        0x11U
#define LINK_MSG_HEALTH       0x12U
#define LINK_MSG_POLICY       0x14U
#define LINK_MSG_CONTROL      0x15U
#define LINK_STATE_ENABLE_OFFSET 16U   /* STATE 负载中 output_enabled 的偏移 */

/* type_frames 下标：STATE、MOTOR、EVENT、HEALTH、POLICY、CONTROL、其他 */
enum { LINK_SLOT_STATE, LINK_SLOT_MOTOR, LINK_SLOT_EVENT, LINK_SLOT_HEALTH,
       LINK_SLOT_POLICY, LINK_SLOT_CONTROL, LINK_SLOT_OTHER, LINK_SLOT_COUNT };

typedef struct {
    uint32_t frames_ok;
    uint32_t state_frames;
    uint32_t type_frames[LINK_SLOT_COUNT];
    uint32_t crc_errors;
    uint32_t length_errors;
    uint32_t seq_lost;          /* frame_seq 跳号累计；含主控 USB 忙时未发出的帧 */
    uint32_t last_tick_ms;      /* 最近一帧 STATE 中主控的 tick_ms */
    bool has_enable;            /* 已收到过 STATE 帧 */
    bool enabled;               /* 最近一帧 STATE 的 output_enabled */
    uint8_t last_version;
    bool has_seq;
    uint16_t last_seq;
    uint16_t size;
    uint8_t buf[LINK_FRAME_MAX_SIZE];
} link_parser_t;

void link_parser_reset(link_parser_t *p);
/* 只清零统计计数，保留未完成的帧、帧号连续性与使能状态（换文件时用） */
void link_parser_reset_counts(link_parser_t *p);
void link_parser_feed(link_parser_t *p, const uint8_t *data, size_t len);
uint16_t link_crc16(const uint8_t *data, size_t len);

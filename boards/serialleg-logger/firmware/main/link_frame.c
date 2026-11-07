#include "link_frame.h"

#include <string.h>

uint16_t link_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFFU;

    while (len--) {
        crc ^= *data++;
        for (int i = 0; i < 8; ++i) {
            crc = (crc & 1U) ? (uint16_t)((crc >> 1) ^ 0x8408U) : (uint16_t)(crc >> 1);
        }
    }
    return crc;
}

static uint16_t read_u16_le(const uint8_t *src)
{
    return (uint16_t)(src[0] | ((uint16_t)src[1] << 8));
}

static uint32_t read_u32_le(const uint8_t *src)
{
    return (uint32_t)src[0] | ((uint32_t)src[1] << 8) | ((uint32_t)src[2] << 16) | ((uint32_t)src[3] << 24);
}

void link_parser_reset(link_parser_t *p)
{
    memset(p, 0, sizeof(*p));
}

void link_parser_reset_counts(link_parser_t *p)
{
    p->frames_ok = 0;
    p->state_frames = 0;
    memset(p->type_frames, 0, sizeof(p->type_frames));
    p->crc_errors = 0;
    p->length_errors = 0;
    p->seq_lost = 0;
}

static unsigned type_slot(uint8_t type)
{
    switch (type) {
    case LINK_MSG_STATE:   return LINK_SLOT_STATE;
    case LINK_MSG_MOTOR:   return LINK_SLOT_MOTOR;
    case LINK_MSG_EVENT:   return LINK_SLOT_EVENT;
    case LINK_MSG_HEALTH:  return LINK_SLOT_HEALTH;
    case LINK_MSG_POLICY:  return LINK_SLOT_POLICY;
    case LINK_MSG_CONTROL: return LINK_SLOT_CONTROL;
    default:               return LINK_SLOT_OTHER;
    }
}

static void handle_frame(link_parser_t *p, uint16_t frame_size)
{
    const uint8_t *f = p->buf;
    uint16_t crc = read_u16_le(&f[frame_size - LINK_CRC_SIZE]);

    if (crc != link_crc16(f, frame_size - LINK_CRC_SIZE)) {
        p->crc_errors++;
        return;
    }

    uint16_t seq = read_u16_le(&f[6]);
    if (p->has_seq) {
        uint16_t step = (uint16_t)(seq - p->last_seq);
        if (step > 1U) {
            p->seq_lost += (uint32_t)(step - 1U);
        }
    }
    p->has_seq = true;
    p->last_seq = seq;
    p->last_version = f[3];
    p->frames_ok++;
    p->type_frames[type_slot(f[2])]++;

    /* STATE 负载首字段为主控 tick_ms，偏移 16 为 output_enabled */
    if (f[2] == LINK_MSG_STATE &&
        frame_size >= LINK_HEADER_SIZE + LINK_STATE_ENABLE_OFFSET + 1U + LINK_CRC_SIZE) {
        p->state_frames++;
        p->last_tick_ms = read_u32_le(&f[LINK_HEADER_SIZE]);
        p->enabled = f[LINK_HEADER_SIZE + LINK_STATE_ENABLE_OFFSET] != 0U;
        p->has_enable = true;
    }
}

void link_parser_feed(link_parser_t *p, const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        uint8_t b = data[i];

        if (p->size == 0U) {
            if (b == LINK_SOF0) {
                p->buf[p->size++] = b;
            }
            continue;
        }
        if (p->size == 1U) {
            if (b == LINK_SOF1) {
                p->buf[p->size++] = b;
            } else if (b != LINK_SOF0) {
                p->size = 0U;
            }
            continue;
        }

        p->buf[p->size++] = b;
        if (p->size < LINK_HEADER_SIZE) {
            continue;
        }

        uint16_t payload_len = read_u16_le(&p->buf[4]);
        if (payload_len > LINK_MAX_PAYLOAD) {
            p->length_errors++;
            p->size = 0U;
            continue;
        }

        uint16_t frame_size = (uint16_t)(LINK_HEADER_SIZE + payload_len + LINK_CRC_SIZE);
        if (p->size == frame_size) {
            handle_frame(p, frame_size);
            p->size = 0U;
        }
    }
}

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CT_MAX_CLASSES          12U
#define CT_STORE_BANK_COUNT      2U
#define CT_STORE_SLOT_COUNT     14U
#define CT_STORE_BANK_SIZE  0x278000U
#define CT_STORE_HEADER_SIZE   0x1000U
#define CT_STORE_SLOT_SIZE    0x2A000U
#define CT_STORE_MAX_PAYLOAD (CT_STORE_SLOT_SIZE - 64U)
#define CT_STORE_PARTITION_MIN (CT_STORE_BANK_COUNT * CT_STORE_BANK_SIZE)

typedef enum {
    CT_STORE_OK = 0,
    CT_STORE_ERR_ARGUMENT = -1,
    CT_STORE_ERR_IO = -2,
    CT_STORE_ERR_FORMAT = -3,
    CT_STORE_ERR_FULL = -4,
    CT_STORE_ERR_STATE = -5,
    CT_STORE_ERR_CRC = -6,
} ct_store_result_t;

typedef struct {
    void *context;
    size_t size;
    int (*read)(void *context, size_t offset, void *data, size_t length);
    int (*write)(void *context, size_t offset, const void *data, size_t length);
    int (*erase)(void *context, size_t offset, size_t length);
} ct_store_backend_t;

typedef struct {
    bool present;
    uint8_t bank;
    uint8_t slot;
    uint16_t class_id;
    uint32_t sequence;
    uint32_t encoded_bytes;
    uint32_t sample_count;
    uint32_t sample_rate;
    uint64_t unix_time;
    uint32_t payload_crc;
} ct_record_info_t;

typedef struct {
    ct_store_backend_t io;
    uint8_t active_bank;
    uint8_t next_slot;
    uint32_t generation;
    uint32_t next_sequence;
    bool stream_open;
    ct_record_info_t latest[CT_MAX_CLASSES];
} ct_store_t;

typedef struct {
    ct_store_t *store;
    uint8_t bank;
    uint8_t slot;
    uint16_t class_id;
    uint32_t sequence;
    uint32_t sample_rate;
    uint32_t encoded_bytes;
    uint32_t payload_crc;
    bool open;
} ct_store_stream_t;

ct_store_result_t ct_store_init(ct_store_t *store, const ct_store_backend_t *backend);
ct_store_result_t ct_store_begin(ct_store_t *store, uint16_t class_id,
                                 uint32_t sample_rate, ct_store_stream_t *stream);
ct_store_result_t ct_store_append(ct_store_stream_t *stream,
                                  const void *data, size_t length);
ct_store_result_t ct_store_read_pending(const ct_store_stream_t *stream,
                                        size_t offset, void *data, size_t length);
ct_store_result_t ct_store_commit(ct_store_stream_t *stream,
                                  uint32_t sample_count, uint64_t unix_time);
void ct_store_abort(ct_store_stream_t *stream);
const ct_record_info_t *ct_store_latest(const ct_store_t *store, uint16_t class_id);
ct_store_result_t ct_store_read_record(const ct_store_t *store,
                                       const ct_record_info_t *record,
                                       size_t offset, void *data, size_t length);
ct_store_result_t ct_store_compact(ct_store_t *store);

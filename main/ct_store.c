#include "ct_store.h"
#include "ct_crc32.h"

#include <stddef.h>
#include <string.h>

#define CT_BANK_MAGIC       0x4354424BU
#define CT_SLOT_MAGIC       0x43545243U
#define CT_COMMIT_MAGIC     0x53415645U
#define CT_FORMAT_VERSION   1U

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t version;
    uint32_t generation;
    uint32_t fields_crc;
    uint32_t commit_magic;
} bank_header_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint16_t class_id;
    uint32_t sequence;
    uint32_t sample_rate;
    uint32_t header_crc;
    uint8_t reserved[12];
} slot_header_t;

typedef struct __attribute__((packed)) {
    uint32_t sequence;
    uint32_t encoded_bytes;
    uint32_t sample_count;
    uint32_t sample_rate;
    uint64_t unix_time;
    uint32_t payload_crc;
    uint32_t fields_crc;
    uint32_t commit_magic;
} slot_trailer_t;

_Static_assert(sizeof(slot_header_t) == 32, "slot header must remain 32 bytes");
_Static_assert(sizeof(slot_trailer_t) <= 32 + 8, "slot trailer budget changed");

static size_t bank_offset(uint8_t bank)
{
    return (size_t)bank * CT_STORE_BANK_SIZE;
}

static size_t slot_offset(uint8_t bank, uint8_t slot)
{
    return bank_offset(bank) + CT_STORE_HEADER_SIZE + (size_t)slot * CT_STORE_SLOT_SIZE;
}

static size_t trailer_offset(uint8_t bank, uint8_t slot)
{
    return slot_offset(bank, slot) + CT_STORE_SLOT_SIZE - sizeof(slot_trailer_t);
}

static uint32_t bank_crc(const bank_header_t *header)
{
    return ct_crc32(header, offsetof(bank_header_t, fields_crc));
}

static uint32_t header_crc(const slot_header_t *header)
{
    return ct_crc32(header, offsetof(slot_header_t, header_crc));
}

static uint32_t trailer_crc(const slot_trailer_t *trailer)
{
    return ct_crc32(trailer, offsetof(slot_trailer_t, fields_crc));
}

static bool io_ok(int result)
{
    return result == 0;
}

static bool bank_header_valid(const bank_header_t *header)
{
    return header->magic == CT_BANK_MAGIC && header->version == CT_FORMAT_VERSION &&
           header->commit_magic == CT_COMMIT_MAGIC && header->fields_crc == bank_crc(header);
}

static bool slot_header_valid(const slot_header_t *header)
{
    return header->magic == CT_SLOT_MAGIC && header->version == CT_FORMAT_VERSION &&
           header->class_id < CT_MAX_CLASSES && header->header_crc == header_crc(header);
}

static bool slot_trailer_valid(const slot_header_t *header, const slot_trailer_t *trailer)
{
    return trailer->commit_magic == CT_COMMIT_MAGIC && trailer->sequence == header->sequence &&
           trailer->sample_rate == header->sample_rate &&
           trailer->encoded_bytes <= CT_STORE_MAX_PAYLOAD &&
           trailer->fields_crc == trailer_crc(trailer);
}

static ct_store_result_t write_bank_header(ct_store_t *store, uint8_t bank, uint32_t generation)
{
    bank_header_t header = {
        .magic = CT_BANK_MAGIC,
        .version = CT_FORMAT_VERSION,
        .generation = generation,
        .commit_magic = CT_COMMIT_MAGIC,
    };
    header.fields_crc = bank_crc(&header);
    return io_ok(store->io.write(store->io.context, bank_offset(bank), &header, sizeof(header)))
               ? CT_STORE_OK : CT_STORE_ERR_IO;
}

static ct_store_result_t payload_crc_for(const ct_store_t *store, uint8_t bank, uint8_t slot,
                                         uint32_t length, uint32_t *crc_out)
{
    uint8_t buffer[512];
    uint32_t crc = 0;
    size_t done = 0;
    while (done < length) {
        size_t chunk = length - done;
        if (chunk > sizeof(buffer)) chunk = sizeof(buffer);
        if (!io_ok(store->io.read(store->io.context,
                                  slot_offset(bank, slot) + sizeof(slot_header_t) + done,
                                  buffer, chunk))) return CT_STORE_ERR_IO;
        crc = ct_crc32_update(crc, buffer, chunk);
        done += chunk;
    }
    *crc_out = crc;
    return CT_STORE_OK;
}

static ct_store_result_t scan_active_bank(ct_store_t *store)
{
    memset(store->latest, 0, sizeof(store->latest));
    store->next_slot = 0;
    store->next_sequence = 1;
    for (uint8_t slot = 0; slot < CT_STORE_SLOT_COUNT; ++slot) {
        slot_header_t header;
        if (!io_ok(store->io.read(store->io.context, slot_offset(store->active_bank, slot),
                                  &header, sizeof(header)))) return CT_STORE_ERR_IO;
        if (header.magic == UINT32_MAX) continue;
        if (store->next_slot <= slot) store->next_slot = slot + 1;
        if (!slot_header_valid(&header)) continue;

        slot_trailer_t trailer;
        if (!io_ok(store->io.read(store->io.context, trailer_offset(store->active_bank, slot),
                                  &trailer, sizeof(trailer)))) return CT_STORE_ERR_IO;
        if (!slot_trailer_valid(&header, &trailer)) continue;
        uint32_t payload_crc = 0;
        ct_store_result_t result = payload_crc_for(store, store->active_bank, slot,
                                                   trailer.encoded_bytes, &payload_crc);
        if (result != CT_STORE_OK) return result;
        if (payload_crc != trailer.payload_crc) continue;

        ct_record_info_t *record = &store->latest[header.class_id];
        if (!record->present || header.sequence > record->sequence) {
            *record = (ct_record_info_t) {
                .present = true,
                .bank = store->active_bank,
                .slot = slot,
                .class_id = header.class_id,
                .sequence = header.sequence,
                .encoded_bytes = trailer.encoded_bytes,
                .sample_count = trailer.sample_count,
                .sample_rate = trailer.sample_rate,
                .unix_time = trailer.unix_time,
                .payload_crc = trailer.payload_crc,
            };
        }
        if (store->next_sequence <= header.sequence) store->next_sequence = header.sequence + 1;
    }
    return CT_STORE_OK;
}

ct_store_result_t ct_store_init(ct_store_t *store, const ct_store_backend_t *backend)
{
    if (!store || !backend || !backend->read || !backend->write || !backend->erase ||
        backend->size < CT_STORE_PARTITION_MIN) return CT_STORE_ERR_ARGUMENT;
    memset(store, 0, sizeof(*store));
    store->io = *backend;

    bank_header_t headers[CT_STORE_BANK_COUNT];
    bool valid[CT_STORE_BANK_COUNT] = { false, false };
    for (uint8_t bank = 0; bank < CT_STORE_BANK_COUNT; ++bank) {
        if (!io_ok(store->io.read(store->io.context, bank_offset(bank),
                                  &headers[bank], sizeof(headers[bank])))) return CT_STORE_ERR_IO;
        valid[bank] = bank_header_valid(&headers[bank]);
    }
    if (!valid[0] && !valid[1]) {
        if (!io_ok(store->io.erase(store->io.context, 0, CT_STORE_BANK_SIZE))) return CT_STORE_ERR_IO;
        ct_store_result_t result = write_bank_header(store, 0, 1);
        if (result != CT_STORE_OK) return result;
        store->active_bank = 0;
        store->generation = 1;
    } else {
        store->active_bank = valid[1] && (!valid[0] || headers[1].generation > headers[0].generation) ? 1 : 0;
        store->generation = headers[store->active_bank].generation;
    }
    return scan_active_bank(store);
}

ct_store_result_t ct_store_begin(ct_store_t *store, uint16_t class_id,
                                 uint32_t sample_rate, ct_store_stream_t *stream)
{
    if (!store || !stream || class_id >= CT_MAX_CLASSES || sample_rate == 0) return CT_STORE_ERR_ARGUMENT;
    if (store->stream_open) return CT_STORE_ERR_STATE;
    if (store->next_slot >= CT_STORE_SLOT_COUNT) {
        ct_store_result_t result = ct_store_compact(store);
        if (result != CT_STORE_OK) return result;
    }
    if (store->next_slot >= CT_STORE_SLOT_COUNT) return CT_STORE_ERR_FULL;

    slot_header_t header = {
        .magic = CT_SLOT_MAGIC,
        .version = CT_FORMAT_VERSION,
        .class_id = class_id,
        .sequence = store->next_sequence++,
        .sample_rate = sample_rate,
    };
    memset(header.reserved, 0xff, sizeof(header.reserved));
    header.header_crc = header_crc(&header);
    uint8_t slot = store->next_slot++;
    if (!io_ok(store->io.write(store->io.context, slot_offset(store->active_bank, slot),
                               &header, sizeof(header)))) return CT_STORE_ERR_IO;
    *stream = (ct_store_stream_t) {
        .store = store,
        .bank = store->active_bank,
        .slot = slot,
        .class_id = class_id,
        .sequence = header.sequence,
        .sample_rate = sample_rate,
        .open = true,
    };
    store->stream_open = true;
    return CT_STORE_OK;
}

ct_store_result_t ct_store_append(ct_store_stream_t *stream, const void *data, size_t length)
{
    if (!stream || !stream->open || !data) return CT_STORE_ERR_ARGUMENT;
    if (length > CT_STORE_MAX_PAYLOAD - stream->encoded_bytes) return CT_STORE_ERR_FULL;
    size_t offset = slot_offset(stream->bank, stream->slot) + sizeof(slot_header_t) + stream->encoded_bytes;
    if (!io_ok(stream->store->io.write(stream->store->io.context, offset, data, length))) return CT_STORE_ERR_IO;
    stream->payload_crc = ct_crc32_update(stream->payload_crc, data, length);
    stream->encoded_bytes += (uint32_t)length;
    return CT_STORE_OK;
}

ct_store_result_t ct_store_read_pending(const ct_store_stream_t *stream,
                                        size_t offset, void *data, size_t length)
{
    if (!stream || !stream->open || !data || offset + length > stream->encoded_bytes) return CT_STORE_ERR_ARGUMENT;
    size_t absolute = slot_offset(stream->bank, stream->slot) + sizeof(slot_header_t) + offset;
    return io_ok(stream->store->io.read(stream->store->io.context, absolute, data, length))
               ? CT_STORE_OK : CT_STORE_ERR_IO;
}

ct_store_result_t ct_store_commit(ct_store_stream_t *stream,
                                  uint32_t sample_count, uint64_t unix_time)
{
    if (!stream || !stream->open || stream->encoded_bytes == 0 || sample_count == 0) return CT_STORE_ERR_STATE;
    slot_trailer_t trailer = {
        .sequence = stream->sequence,
        .encoded_bytes = stream->encoded_bytes,
        .sample_count = sample_count,
        .sample_rate = stream->sample_rate,
        .unix_time = unix_time,
        .payload_crc = stream->payload_crc,
        .commit_magic = UINT32_MAX,
    };
    trailer.fields_crc = trailer_crc(&trailer);
    size_t offset = trailer_offset(stream->bank, stream->slot);
    if (!io_ok(stream->store->io.write(stream->store->io.context, offset, &trailer,
                                       offsetof(slot_trailer_t, commit_magic)))) return CT_STORE_ERR_IO;
    uint32_t commit = CT_COMMIT_MAGIC;
    if (!io_ok(stream->store->io.write(stream->store->io.context,
                                       offset + offsetof(slot_trailer_t, commit_magic),
                                       &commit, sizeof(commit)))) return CT_STORE_ERR_IO;

    ct_record_info_t *record = &stream->store->latest[stream->class_id];
    *record = (ct_record_info_t) {
        .present = true, .bank = stream->bank, .slot = stream->slot,
        .class_id = stream->class_id, .sequence = stream->sequence,
        .encoded_bytes = stream->encoded_bytes, .sample_count = sample_count,
        .sample_rate = stream->sample_rate, .unix_time = unix_time,
        .payload_crc = stream->payload_crc,
    };
    stream->open = false;
    stream->store->stream_open = false;
    return CT_STORE_OK;
}

void ct_store_abort(ct_store_stream_t *stream)
{
    if (!stream || !stream->open) return;
    stream->open = false;
    stream->store->stream_open = false;
}

const ct_record_info_t *ct_store_latest(const ct_store_t *store, uint16_t class_id)
{
    if (!store || class_id >= CT_MAX_CLASSES || !store->latest[class_id].present) return NULL;
    return &store->latest[class_id];
}

ct_store_result_t ct_store_read_record(const ct_store_t *store,
                                       const ct_record_info_t *record,
                                       size_t offset, void *data, size_t length)
{
    if (!store || !record || !record->present || !data || offset + length > record->encoded_bytes) return CT_STORE_ERR_ARGUMENT;
    size_t absolute = slot_offset(record->bank, record->slot) + sizeof(slot_header_t) + offset;
    return io_ok(store->io.read(store->io.context, absolute, data, length))
               ? CT_STORE_OK : CT_STORE_ERR_IO;
}

ct_store_result_t ct_store_compact(ct_store_t *store)
{
    if (!store || store->stream_open) return CT_STORE_ERR_STATE;
    uint8_t target = (uint8_t)(1U - store->active_bank);
    if (!io_ok(store->io.erase(store->io.context, bank_offset(target), CT_STORE_BANK_SIZE))) return CT_STORE_ERR_IO;

    uint8_t target_slot = 0;
    uint8_t buffer[512];
    for (uint16_t class_id = 0; class_id < CT_MAX_CLASSES; ++class_id) {
        const ct_record_info_t *record = &store->latest[class_id];
        if (!record->present) continue;
        if (target_slot >= CT_STORE_SLOT_COUNT) return CT_STORE_ERR_FULL;

        slot_header_t header = {
            .magic = CT_SLOT_MAGIC, .version = CT_FORMAT_VERSION,
            .class_id = class_id, .sequence = record->sequence,
            .sample_rate = record->sample_rate,
        };
        memset(header.reserved, 0xff, sizeof(header.reserved));
        header.header_crc = header_crc(&header);
        if (!io_ok(store->io.write(store->io.context, slot_offset(target, target_slot),
                                   &header, sizeof(header)))) return CT_STORE_ERR_IO;
        size_t copied = 0;
        while (copied < record->encoded_bytes) {
            size_t chunk = record->encoded_bytes - copied;
            if (chunk > sizeof(buffer)) chunk = sizeof(buffer);
            ct_store_result_t result = ct_store_read_record(store, record, copied, buffer, chunk);
            if (result != CT_STORE_OK) return result;
            if (!io_ok(store->io.write(store->io.context,
                                       slot_offset(target, target_slot) + sizeof(slot_header_t) + copied,
                                       buffer, chunk))) return CT_STORE_ERR_IO;
            copied += chunk;
        }
        slot_trailer_t trailer = {
            .sequence = record->sequence, .encoded_bytes = record->encoded_bytes,
            .sample_count = record->sample_count, .sample_rate = record->sample_rate,
            .unix_time = record->unix_time, .payload_crc = record->payload_crc,
            .commit_magic = CT_COMMIT_MAGIC,
        };
        trailer.fields_crc = trailer_crc(&trailer);
        if (!io_ok(store->io.write(store->io.context, trailer_offset(target, target_slot),
                                   &trailer, sizeof(trailer)))) return CT_STORE_ERR_IO;
        target_slot++;
    }
    ct_store_result_t result = write_bank_header(store, target, store->generation + 1U);
    if (result != CT_STORE_OK) return result;
    store->active_bank = target;
    store->generation++;
    return scan_active_bank(store);
}

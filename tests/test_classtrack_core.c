#include "ct_adpcm.h"
#include "ct_classes.h"
#include "ct_protocol.h"
#include "ct_store.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { unsigned char *data; size_t size; } memory_flash_t;

static int mem_read(void *context, size_t offset, void *data, size_t length)
{
    memory_flash_t *flash = context;
    if (offset + length > flash->size) return -1;
    memcpy(data, flash->data + offset, length); return 0;
}

static int mem_write(void *context, size_t offset, const void *data, size_t length)
{
    memory_flash_t *flash = context; const unsigned char *input = data;
    if (offset + length > flash->size) return -1;
    for (size_t i = 0; i < length; ++i) {
        if ((flash->data[offset + i] & input[i]) != input[i]) return -1;
        flash->data[offset + i] &= input[i];
    }
    return 0;
}

static int mem_erase(void *context, size_t offset, size_t length)
{
    memory_flash_t *flash = context;
    if (offset + length > flash->size) return -1;
    memset(flash->data + offset, 0xff, length); return 0;
}

static void test_adpcm(void)
{
    int16_t input[CT_ADPCM_SAMPLES_PER_BLOCK], output[CT_ADPCM_SAMPLES_PER_BLOCK];
    for (size_t i = 0; i < CT_ADPCM_SAMPLES_PER_BLOCK; ++i)
        input[i] = (int16_t)(((int)i - 128) * 180);
    uint8_t encoded[CT_ADPCM_BYTES_PER_BLOCK];
    size_t bytes = ct_adpcm_encode_block(input, CT_ADPCM_SAMPLES_PER_BLOCK, encoded);
    assert(bytes == CT_ADPCM_BYTES_PER_BLOCK);
    assert(ct_adpcm_decode_block(encoded, bytes, output) == CT_ADPCM_SAMPLES_PER_BLOCK);
    long long error = 0;
    for (size_t i = 0; i < CT_ADPCM_SAMPLES_PER_BLOCK; ++i) error += llabs((long long)input[i] - output[i]);
    assert(error / CT_ADPCM_SAMPLES_PER_BLOCK < 1200);
    assert(ct_adpcm_max_encoded_bytes(16000U * 20U) == 165000U);
}

static void append_record(ct_store_t *store, uint16_t class_id, const char *text, uint64_t timestamp)
{
    ct_store_stream_t stream;
    assert(ct_store_begin(store, class_id, 16000, &stream) == CT_STORE_OK);
    assert(ct_store_append(&stream, text, strlen(text)) == CT_STORE_OK);
    assert(ct_store_commit(&stream, 256, timestamp) == CT_STORE_OK);
}

static void test_store(void)
{
    memory_flash_t flash = { .size = CT_STORE_PARTITION_MIN };
    flash.data = malloc(flash.size); assert(flash.data); memset(flash.data, 0xff, flash.size);
    ct_store_backend_t backend = { &flash, flash.size, mem_read, mem_write, mem_erase };
    ct_store_t store; assert(ct_store_init(&store, &backend) == CT_STORE_OK);
    append_record(&store, 0, "old", 100);
    const ct_record_info_t *record = ct_store_latest(&store, 0); assert(record && record->sequence == 1);

    ct_store_stream_t interrupted;
    assert(ct_store_begin(&store, 0, 16000, &interrupted) == CT_STORE_OK);
    assert(ct_store_append(&interrupted, "incomplete", 10) == CT_STORE_OK);
    ct_store_abort(&interrupted);

    ct_store_t rebooted; assert(ct_store_init(&rebooted, &backend) == CT_STORE_OK);
    record = ct_store_latest(&rebooted, 0); assert(record && record->sequence == 1);
    char payload[4] = {0}; assert(ct_store_read_record(&rebooted, record, 0, payload, 3) == CT_STORE_OK);
    assert(strcmp(payload, "old") == 0);

    append_record(&rebooted, 0, "new", 200);
    for (unsigned i = 0; i < 13; ++i) append_record(&rebooted, (uint16_t)(i % 4), "x", 300 + i);
    assert(rebooted.generation >= 2);
    for (unsigned i = 0; i < 4; ++i) assert(ct_store_latest(&rebooted, (uint16_t)i));
    free(flash.data);
}

static void test_csv_and_protocol(void)
{
    const char csv[] = "id,name,course\n0,26机电2班,高职英语\n1,26语言1班,汉语听力\n";
    ct_class_list_t list; char error[80];
    assert(ct_classes_parse_csv(csv, strlen(csv), &list, error, sizeof(error)) == 0);
    assert(list.count == 2 && list.items[1].id == 1);
    const char duplicate[] = "id,name,course\n0,A,B\n0,C,D\n";
    assert(ct_classes_parse_csv(duplicate, strlen(duplicate), &list, error, sizeof(error)) != 0);

    ct_protocol_command_t command;
    assert(ct_protocol_parse_line("TIME 1793021000", &command) == 0 && command.type == CT_PROTO_TIME);
    assert(ct_protocol_parse_line("IMPORT 123 89abcdef", &command) == 0 && command.length == 123);
    assert(ct_protocol_parse_line("GET 11", &command) == 0 && command.class_id == 11);
    assert(ct_protocol_parse_line("GET 12", &command) != 0);
}

typedef struct { int x, y, w, h; } rect_t;
static int overlaps(rect_t a, rect_t b)
{
    return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
}

static void test_layout(void)
{
    rect_t rows[7];
    for (int i = 0; i < 7; ++i) rows[i] = (rect_t){ 10, 88 + i * 28, 220, 27 };
    for (int i = 0; i < 7; ++i) {
        assert(rows[i].x >= 0 && rows[i].x + rows[i].w <= 240);
        assert(rows[i].y >= 34 && rows[i].y + rows[i].h <= 290);
        for (int j = i + 1; j < 7; ++j) assert(!overlaps(rows[i], rows[j]));
    }
    rect_t review_actions[4];
    for (int i = 0; i < 4; ++i) review_actions[i] = (rect_t){ 10, 125 + i * 31, 220, 27 };
    rect_t review_note = { 12, 258, 216, 20 };
    for (int i = 0; i < 4; ++i) assert(!overlaps(review_actions[i], review_note));
    assert(review_note.y + review_note.h <= 290);
}

int main(void)
{
    test_adpcm(); test_store(); test_csv_and_protocol(); test_layout();
    puts("PASS: ClassTrack codec, transaction store, CSV/protocol, and 240x320 layout tests");
    return 0;
}

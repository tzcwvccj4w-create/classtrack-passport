#include "ct_audio.h"
#include "ct_adpcm.h"

#include "bsp_audio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <string.h>

#define SAMPLE_RATE 16000U
#define MAX_SAMPLES (SAMPLE_RATE * 20U)
#define AUDIO_QUEUE_DEPTH 4

typedef enum { CMD_RECORD = 1, CMD_PREVIEW, CMD_PLAY_LATEST } command_type_t;
typedef struct { command_type_t type; uint16_t class_id; } command_t;

static const char *TAG = "ct_audio";
static ct_store_t *s_store;
static ct_store_stream_t s_pending;
static ct_audio_event_cb_t s_callback;
static void *s_callback_user;
static QueueHandle_t s_queue;
static volatile bool s_busy;
static volatile bool s_stop_record;
static volatile uint32_t s_pending_samples;

static void emit(ct_audio_event_t event, unsigned seconds)
{
    if (s_callback) s_callback(event, seconds, s_callback_user);
}

static int play_reader(bool pending, const ct_record_info_t *record)
{
    if (bsp_audio_set_format(SAMPLE_RATE, 16, 1) != ESP_OK) return -1;
    bsp_audio_set_volume(75);
    uint8_t encoded[CT_ADPCM_BYTES_PER_BLOCK];
    int16_t pcm[CT_ADPCM_SAMPLES_PER_BLOCK];
    size_t total = pending ? s_pending.encoded_bytes : record->encoded_bytes;
    size_t offset = 0;
    while (offset < total) {
        size_t chunk = total - offset;
        if (chunk > sizeof(encoded)) chunk = sizeof(encoded);
        ct_store_result_t result = pending
            ? ct_store_read_pending(&s_pending, offset, encoded, chunk)
            : ct_store_read_record(s_store, record, offset, encoded, chunk);
        if (result != CT_STORE_OK) return -2;
        size_t samples = ct_adpcm_decode_block(encoded, chunk, pcm);
        if (samples == 0 || bsp_audio_write(pcm, samples * sizeof(int16_t)) != ESP_OK) return -3;
        offset += chunk;
    }
    return 0;
}

static int record_stream(uint16_t class_id)
{
    if (s_pending.open) ct_store_abort(&s_pending);
    if (ct_store_begin(s_store, class_id, SAMPLE_RATE, &s_pending) != CT_STORE_OK) return -1;
    if (bsp_audio_set_format(SAMPLE_RATE, 16, 1) != ESP_OK) { ct_store_abort(&s_pending); return -2; }
    s_pending_samples = 0;
    int16_t pcm[CT_ADPCM_SAMPLES_PER_BLOCK];
    uint8_t encoded[CT_ADPCM_BYTES_PER_BLOCK];
    while (!s_stop_record && s_pending_samples < MAX_SAMPLES) {
        if (bsp_audio_read(pcm, sizeof(pcm)) != ESP_OK) { ct_store_abort(&s_pending); return -3; }
        size_t bytes = ct_adpcm_encode_block(pcm, CT_ADPCM_SAMPLES_PER_BLOCK, encoded);
        if (!bytes || ct_store_append(&s_pending, encoded, bytes) != CT_STORE_OK) { ct_store_abort(&s_pending); return -4; }
        s_pending_samples += CT_ADPCM_SAMPLES_PER_BLOCK;
    }
    return 0;
}

static void audio_task(void *arg)
{
    (void)arg;
    command_t command;
    for (;;) {
        if (xQueueReceive(s_queue, &command, portMAX_DELAY) != pdTRUE) continue;
        s_busy = true;
        int result = 0;
        if (command.type == CMD_RECORD) result = record_stream(command.class_id);
        else if (command.type == CMD_PREVIEW) result = s_pending.open ? play_reader(true, NULL) : -1;
        else if (command.type == CMD_PLAY_LATEST) {
            const ct_record_info_t *record = ct_store_latest(s_store, command.class_id);
            result = record ? play_reader(false, record) : -1;
        }
        s_busy = false;
        if (result != 0) {
            ESP_LOGE(TAG, "audio command %d failed: %d", command.type, result);
            emit(CT_AUDIO_EVENT_ERROR, 0);
        } else if (command.type == CMD_RECORD) {
            emit(CT_AUDIO_EVENT_RECORD_READY, s_pending_samples / SAMPLE_RATE);
        } else emit(CT_AUDIO_EVENT_PLAY_DONE, 0);
    }
}

int ct_audio_init(ct_store_t *store, ct_audio_event_cb_t callback, void *user)
{
    if (!store || s_queue) return -1;
    s_store = store; s_callback = callback; s_callback_user = user;
    s_queue = xQueueCreate(AUDIO_QUEUE_DEPTH, sizeof(command_t));
    if (!s_queue) return -2;
    if (xTaskCreate(audio_task, "ct_audio", 4096, NULL, 5, NULL) != pdPASS) {
        vQueueDelete(s_queue); s_queue = NULL; return -3;
    }
    return 0;
}

static int send(command_type_t type, uint16_t class_id)
{
    if (!s_queue || s_busy) return -1;
    command_t command = { .type = type, .class_id = class_id };
    return xQueueSend(s_queue, &command, 0) == pdTRUE ? 0 : -2;
}

int ct_audio_start_record(uint16_t class_id)
{
    s_stop_record = false;
    return send(CMD_RECORD, class_id);
}
void ct_audio_stop_record(void) { s_stop_record = true; }
int ct_audio_preview(void) { return send(CMD_PREVIEW, 0); }
int ct_audio_play_latest(uint16_t class_id) { return send(CMD_PLAY_LATEST, class_id); }

int ct_audio_commit(uint64_t unix_time)
{
    if (s_busy || !s_pending.open) return -1;
    return ct_store_commit(&s_pending, s_pending_samples, unix_time) == CT_STORE_OK ? 0 : -2;
}

void ct_audio_discard(void)
{
    if (!s_busy && s_pending.open) ct_store_abort(&s_pending);
    s_pending_samples = 0;
}

bool ct_audio_is_busy(void) { return s_busy; }
unsigned ct_audio_pending_seconds(void) { return s_pending_samples / SAMPLE_RATE; }

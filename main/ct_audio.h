#pragma once

#include "ct_store.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    CT_AUDIO_EVENT_RECORD_READY = 1,
    CT_AUDIO_EVENT_PLAY_DONE,
    CT_AUDIO_EVENT_ERROR,
} ct_audio_event_t;

typedef void (*ct_audio_event_cb_t)(ct_audio_event_t event, unsigned seconds, void *user);

int ct_audio_init(ct_store_t *store, ct_audio_event_cb_t callback, void *user);
int ct_audio_start_record(uint16_t class_id);
void ct_audio_stop_record(void);
int ct_audio_preview(void);
int ct_audio_play_latest(uint16_t class_id);
int ct_audio_commit(uint64_t unix_time);
void ct_audio_discard(void);
bool ct_audio_is_busy(void);
unsigned ct_audio_pending_seconds(void);

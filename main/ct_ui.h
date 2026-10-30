#pragma once

#include "ct_classes.h"
#include "ct_store.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    CT_SCREEN_HOME = 0,
    CT_SCREEN_DETAIL,
    CT_SCREEN_RECORDING,
    CT_SCREEN_REVIEW,
    CT_SCREEN_RECENT,
    CT_SCREEN_SETTINGS,
    CT_SCREEN_TIME_SYNC,
    CT_SCREEN_AUDIO_ERROR,
} ct_screen_t;

void ct_ui_init(const ct_class_list_t *classes, const ct_store_t *store);
void ct_ui_show(ct_screen_t screen, unsigned selected, unsigned class_index,
                unsigned seconds, int battery_soc, uint64_t unix_time);
bool ct_ui_font_supports_text(const char *utf8);

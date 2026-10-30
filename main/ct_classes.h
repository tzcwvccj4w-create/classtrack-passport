#pragma once

#include "ct_store.h"

#include <stddef.h>
#include <stdint.h>

#define CT_CLASS_NAME_BYTES   40U
#define CT_COURSE_NAME_BYTES  40U

typedef struct {
    uint16_t id;
    char name[CT_CLASS_NAME_BYTES];
    char course[CT_COURSE_NAME_BYTES];
} ct_class_t;

typedef struct {
    uint32_t version;
    uint16_t count;
    ct_class_t items[CT_MAX_CLASSES];
    uint32_t crc32;
} ct_class_list_t;

int ct_classes_parse_csv(const char *csv, size_t length, ct_class_list_t *list,
                         char *error, size_t error_size);
int ct_classes_load(ct_class_list_t *list);
int ct_classes_save(const ct_class_list_t *list);

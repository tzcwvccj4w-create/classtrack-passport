#include "ct_classes.h"
#include "ct_crc32.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef CT_HOST_TEST
#include "nvs.h"
#endif

#define CT_CLASSES_VERSION 1U

static bool utf8_valid(const char *text)
{
    const unsigned char *p = (const unsigned char *)text;
    while (*p) {
        if (*p < 0x80) { p++; continue; }
        unsigned continuation;
        if ((*p & 0xe0) == 0xc0) continuation = 1;
        else if ((*p & 0xf0) == 0xe0) continuation = 2;
        else if ((*p & 0xf8) == 0xf0) continuation = 3;
        else return false;
        p++;
        while (continuation--) if ((*p++ & 0xc0) != 0x80) return false;
    }
    return true;
}

static void set_error(char *error, size_t size, const char *message)
{
    if (error && size) snprintf(error, size, "%s", message);
}

static char *trim(char *text)
{
    while (*text == ' ' || *text == '\t') text++;
    size_t length = strlen(text);
    while (length && (text[length - 1] == ' ' || text[length - 1] == '\t' || text[length - 1] == '\r')) text[--length] = '\0';
    return text;
}

int ct_classes_parse_csv(const char *csv, size_t length, ct_class_list_t *list,
                         char *error, size_t error_size)
{
    if (!csv || !list || length == 0 || length > 4096) {
        set_error(error, error_size, "invalid CSV size");
        return -1;
    }
    char *copy = malloc(length + 1U);
    if (!copy) return -2;
    memcpy(copy, csv, length);
    copy[length] = '\0';

    ct_class_list_t parsed = { .version = CT_CLASSES_VERSION };
    bool first = true;
    char *save_line = NULL;
    for (char *line = strtok_r(copy, "\n", &save_line); line; line = strtok_r(NULL, "\n", &save_line)) {
        if (first) {
            first = false;
            if (strncmp(line, "id,", 3) == 0) continue;
        }
        if (!*trim(line)) continue;
        if (parsed.count >= CT_MAX_CLASSES) { set_error(error, error_size, "too many classes"); free(copy); return -3; }
        char *save_field = NULL;
        char *id_text = trim(strtok_r(line, ",", &save_field));
        char *name = strtok_r(NULL, ",", &save_field);
        char *course = strtok_r(NULL, ",", &save_field);
        char *extra = strtok_r(NULL, ",", &save_field);
        if (!id_text || !name || !course || extra) { set_error(error, error_size, "expected id,name,course"); free(copy); return -4; }
        name = trim(name); course = trim(course);
        char *end = NULL;
        long id = strtol(id_text, &end, 10);
        if (!*id_text || *end || id < 0 || id >= (long)CT_MAX_CLASSES) { set_error(error, error_size, "class id must be 0..11"); free(copy); return -5; }
        if (!*name || strlen(name) >= CT_CLASS_NAME_BYTES || strlen(course) >= CT_COURSE_NAME_BYTES ||
            !utf8_valid(name) || !utf8_valid(course)) { set_error(error, error_size, "invalid or long UTF-8 field"); free(copy); return -6; }
        for (uint16_t i = 0; i < parsed.count; ++i) if (parsed.items[i].id == (uint16_t)id) {
            set_error(error, error_size, "duplicate class id"); free(copy); return -7;
        }
        ct_class_t *item = &parsed.items[parsed.count++];
        item->id = (uint16_t)id;
        snprintf(item->name, sizeof(item->name), "%s", name);
        snprintf(item->course, sizeof(item->course), "%s", course);
    }
    free(copy);
    if (parsed.count == 0) { set_error(error, error_size, "no classes"); return -8; }
    parsed.crc32 = ct_crc32(&parsed, offsetof(ct_class_list_t, crc32));
    *list = parsed;
    return 0;
}

#ifndef CT_HOST_TEST
int ct_classes_load(ct_class_list_t *list)
{
    if (!list) return -1;
    nvs_handle_t handle;
    if (nvs_open("classtrack", NVS_READONLY, &handle) != ESP_OK) return -2;
    size_t size = sizeof(*list);
    esp_err_t err = nvs_get_blob(handle, "classes", list, &size);
    nvs_close(handle);
    if (err != ESP_OK || size != sizeof(*list) || list->version != CT_CLASSES_VERSION ||
        list->count > CT_MAX_CLASSES || list->crc32 != ct_crc32(list, offsetof(ct_class_list_t, crc32))) return -3;
    return 0;
}

int ct_classes_save(const ct_class_list_t *list)
{
    if (!list || list->count == 0 || list->count > CT_MAX_CLASSES) return -1;
    ct_class_list_t copy = *list;
    copy.version = CT_CLASSES_VERSION;
    copy.crc32 = ct_crc32(&copy, offsetof(ct_class_list_t, crc32));
    nvs_handle_t handle;
    if (nvs_open("classtrack", NVS_READWRITE, &handle) != ESP_OK) return -2;
    esp_err_t err = nvs_set_blob(handle, "classes", &copy, sizeof(copy));
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err == ESP_OK ? 0 : -3;
}
#else
int ct_classes_load(ct_class_list_t *list) { (void)list; return -1; }
int ct_classes_save(const ct_class_list_t *list) { (void)list; return -1; }
#endif

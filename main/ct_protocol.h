#pragma once

#include <stddef.h>
#include <stdint.h>

typedef enum {
    CT_PROTO_INVALID = 0,
    CT_PROTO_HELLO,
    CT_PROTO_TIME,
    CT_PROTO_IMPORT,
    CT_PROTO_LIST,
    CT_PROTO_GET,
} ct_protocol_command_type_t;

typedef struct {
    ct_protocol_command_type_t type;
    uint64_t value;
    uint32_t length;
    uint32_t crc32;
    uint16_t class_id;
} ct_protocol_command_t;

int ct_protocol_parse_line(const char *line, ct_protocol_command_t *command);

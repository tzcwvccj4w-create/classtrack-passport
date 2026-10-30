#include "ct_protocol.h"

#include <stdio.h>
#include <string.h>

int ct_protocol_parse_line(const char *line, ct_protocol_command_t *command)
{
    if (!line || !command) return -1;
    memset(command, 0, sizeof(*command));
    if (strcmp(line, "HELLO") == 0) command->type = CT_PROTO_HELLO;
    else if (strcmp(line, "LIST") == 0) command->type = CT_PROTO_LIST;
    else {
        unsigned long long epoch = 0;
        if (sscanf(line, "TIME %llu", &epoch) == 1) {
            command->type = CT_PROTO_TIME;
            command->value = (uint64_t)epoch;
            return 0;
        }
        unsigned length = 0, crc = 0, id = 0;
        char tail = '\0';
        if (sscanf(line, "IMPORT %u %x %c", &length, &crc, &tail) == 2 && length <= 4096U) {
            command->type = CT_PROTO_IMPORT; command->length = length; command->crc32 = crc;
        } else if (sscanf(line, "GET %u %c", &id, &tail) == 1 && id < 12U) {
            command->type = CT_PROTO_GET; command->class_id = (uint16_t)id;
        } else return -2;
    }
    return 0;
}

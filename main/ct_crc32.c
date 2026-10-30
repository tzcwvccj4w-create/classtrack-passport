#include "ct_crc32.h"

uint32_t ct_crc32_update(uint32_t crc, const void *data, size_t length)
{
    const uint8_t *bytes = data;
    crc = ~crc;
    for (size_t i = 0; i < length; ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit) {
            uint32_t mask = (uint32_t)-(int32_t)(crc & 1U);
            crc = (crc >> 1) ^ (0xEDB88320U & mask);
        }
    }
    return ~crc;
}

uint32_t ct_crc32(const void *data, size_t length)
{
    return ct_crc32_update(0, data, length);
}

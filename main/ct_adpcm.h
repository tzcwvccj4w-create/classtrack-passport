#pragma once

#include <stddef.h>
#include <stdint.h>

#define CT_ADPCM_SAMPLES_PER_BLOCK 256U
#define CT_ADPCM_BYTES_PER_BLOCK   132U

typedef struct {
    int16_t predictor;
    uint8_t step_index;
} ct_adpcm_state_t;

/*
 * Each independent block stores a 4-byte predictor/index header followed by
 * 255 4-bit deltas. Independent blocks bound seek/recovery damage and let the
 * ES8311 worker encode/decode with one 512-byte PCM buffer.
 */
size_t ct_adpcm_encode_block(const int16_t *pcm, size_t sample_count,
                             uint8_t output[CT_ADPCM_BYTES_PER_BLOCK]);
size_t ct_adpcm_decode_block(const uint8_t *input, size_t input_bytes,
                             int16_t pcm[CT_ADPCM_SAMPLES_PER_BLOCK]);
size_t ct_adpcm_max_encoded_bytes(size_t sample_count);

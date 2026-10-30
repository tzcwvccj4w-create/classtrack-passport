#include "ct_adpcm.h"

#include <string.h>

static const int s_index_table[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8,
};

static const int s_step_table[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143,
    157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544,
    598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878,
    2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894,
    6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818,
    18500, 20350, 22385, 24623, 27086, 29794, 32767,
};

static int clamp_int(int value, int low, int high)
{
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static uint8_t encode_nibble(int16_t sample, ct_adpcm_state_t *state)
{
    int predictor = state->predictor;
    int step = s_step_table[state->step_index];
    int delta = (int)sample - predictor;
    uint8_t code = 0;
    if (delta < 0) {
        code = 8;
        delta = -delta;
    }

    int diff = step >> 3;
    if (delta >= step) { code |= 4; delta -= step; diff += step; }
    if (delta >= (step >> 1)) { code |= 2; delta -= step >> 1; diff += step >> 1; }
    if (delta >= (step >> 2)) { code |= 1; diff += step >> 2; }

    predictor += (code & 8) ? -diff : diff;
    state->predictor = (int16_t)clamp_int(predictor, -32768, 32767);
    state->step_index = (uint8_t)clamp_int((int)state->step_index + s_index_table[code], 0, 88);
    return code;
}

static int16_t decode_nibble(uint8_t code, ct_adpcm_state_t *state)
{
    int step = s_step_table[state->step_index];
    int diff = step >> 3;
    if (code & 4) diff += step;
    if (code & 2) diff += step >> 1;
    if (code & 1) diff += step >> 2;
    int predictor = state->predictor + ((code & 8) ? -diff : diff);
    state->predictor = (int16_t)clamp_int(predictor, -32768, 32767);
    state->step_index = (uint8_t)clamp_int((int)state->step_index + s_index_table[code], 0, 88);
    return state->predictor;
}

size_t ct_adpcm_encode_block(const int16_t *pcm, size_t sample_count,
                             uint8_t output[CT_ADPCM_BYTES_PER_BLOCK])
{
    if (!pcm || !output || sample_count == 0 || sample_count > CT_ADPCM_SAMPLES_PER_BLOCK) return 0;
    memset(output, 0, CT_ADPCM_BYTES_PER_BLOCK);
    ct_adpcm_state_t state = { .predictor = pcm[0], .step_index = 0 };
    output[0] = (uint8_t)(state.predictor & 0xff);
    output[1] = (uint8_t)((uint16_t)state.predictor >> 8);
    output[2] = state.step_index;
    output[3] = (uint8_t)(sample_count - 1U);

    for (size_t i = 1; i < sample_count; ++i) {
        uint8_t code = encode_nibble(pcm[i], &state);
        size_t byte_index = 4U + (i - 1U) / 2U;
        if ((i & 1U) != 0) output[byte_index] = code;
        else output[byte_index] |= (uint8_t)(code << 4);
    }
    return 4U + sample_count / 2U;
}

size_t ct_adpcm_decode_block(const uint8_t *input, size_t input_bytes,
                             int16_t pcm[CT_ADPCM_SAMPLES_PER_BLOCK])
{
    if (!input || !pcm || input_bytes < 4) return 0;
    size_t sample_count = (size_t)input[3] + 1U;
    if (sample_count == 0 || sample_count > CT_ADPCM_SAMPLES_PER_BLOCK ||
        input_bytes < 4U + sample_count / 2U) return 0;

    ct_adpcm_state_t state = {
        .predictor = (int16_t)((uint16_t)input[0] | ((uint16_t)input[1] << 8)),
        .step_index = input[2],
    };
    if (state.step_index > 88) return 0;
    pcm[0] = state.predictor;
    for (size_t i = 1; i < sample_count; ++i) {
        uint8_t packed = input[4U + (i - 1U) / 2U];
        uint8_t code = (i & 1U) ? (packed & 0x0fU) : (packed >> 4);
        pcm[i] = decode_nibble(code, &state);
    }
    return sample_count;
}

size_t ct_adpcm_max_encoded_bytes(size_t sample_count)
{
    size_t blocks = (sample_count + CT_ADPCM_SAMPLES_PER_BLOCK - 1U) /
                    CT_ADPCM_SAMPLES_PER_BLOCK;
    return blocks * CT_ADPCM_BYTES_PER_BLOCK;
}

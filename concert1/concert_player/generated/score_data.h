#ifndef SCORE_DATA_H
#define SCORE_DATA_H
#include <stdint.h>
typedef struct {
    const uint8_t *data;
    uint32_t size;
    uint32_t duration_ms;
} Score;
typedef struct { uint16_t prescaler; uint16_t reload; } PitchTimer;
extern const Score scores[3];
extern const PitchTimer pitch_timers[128];
#endif

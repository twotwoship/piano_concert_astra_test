#include "player.h"
#include "buzzer.h"
#include "generated/score_data.h"

static PlayerState state;
static uint8_t movement, sounding[8], frame_notes[8], frame_mask;
static uint32_t cursor, due, position, last_wall, gap_left;
static uint8_t frame_valid;

static void Fail(void)
{
    state = PLAYER_ERROR;
    frame_valid = 0U;
    Buzzer_Silence();
}

static int Read_Byte(uint8_t *value)
{
    const Score *s = &scores[movement];
    if (cursor >= s->size) { Fail(); return 0; }
    *value = s->data[cursor++];
    return 1;
}

static void Read_Frame(void)
{
    const Score *s = &scores[movement];
    frame_valid = 0U;
    if (cursor == s->size) return;
    uint8_t b;
    uint32_t delta = 0U, count = 0U;
    do {
        if (++count > 4U || !Read_Byte(&b)) { Fail(); return; }
        delta = (delta << 7) | (b & 127U);
    } while (b & 128U);
    if (delta > s->duration_ms || due > s->duration_ms - delta) { Fail(); return; }
    due += delta;
    if (!Read_Byte(&frame_mask)) return;
    for (uint8_t i = 0; i < 8U; ++i) {
        if (frame_mask & (1U << i)) {
            if (!Read_Byte(&frame_notes[i])) return;
            if (frame_notes[i] > 128U) { Fail(); return; }
        }
    }
    frame_valid = 1U;
}

static void Reset_Movement(void)
{
    Buzzer_Silence();
    for (uint8_t i = 0; i < 8U; ++i) sounding[i] = 0U;
    cursor = due = position = gap_left = 0U;
    state = PLAYER_READY;
    Read_Frame();
}

void Player_Init(void)
{
    movement = 0U;
    last_wall = 0U;
    Reset_Movement();
}

void Player_Toggle(uint32_t now)
{
    if (state == PLAYER_PLAYING) {
        Player_Update(now);
        if (state != PLAYER_PLAYING) return;
        state = PLAYER_PAUSED;
        Buzzer_Silence();
    } else if (state == PLAYER_PAUSED) {
        last_wall = now;
        state = PLAYER_PLAYING;
        for (uint8_t i = 0; i < 8U; ++i) Buzzer_Set(i, sounding[i]);
    } else {
        if (state == PLAYER_FINISHED) movement = 0U;
        Reset_Movement();
        if (state == PLAYER_ERROR) return;
        state = PLAYER_PLAYING;
        last_wall = now;
        Player_Update(now);
    }
}

void Player_Next(uint32_t now)
{
    movement = (movement + 1U) % 3U;
    last_wall = now;
    Reset_Movement();
}

void Player_Update(uint32_t now)
{
    if (state != PLAYER_PLAYING) return;
    uint32_t elapsed = now - last_wall; /* SysTick wrap-safe subtraction */
    last_wall = now;
    if (gap_left) {
        if (elapsed < gap_left) { gap_left -= elapsed; return; }
        elapsed -= gap_left;
        ++movement;
        Reset_Movement();
        if (state == PLAYER_ERROR) return;
        state = PLAYER_PLAYING;
    }
    position += elapsed;
    while (frame_valid && due <= position && state == PLAYER_PLAYING) {
        for (uint8_t i = 0; i < 8U; ++i) {
            if (frame_mask & (1U << i)) {
                sounding[i] = frame_notes[i];
                Buzzer_Set(i, sounding[i]);
            }
        }
        Read_Frame();
    }
    if (state == PLAYER_ERROR) return;
    if (position >= scores[movement].duration_ms) {
        position = scores[movement].duration_ms;
        Buzzer_Silence();
        for (uint8_t i = 0; i < 8U; ++i) sounding[i] = 0U;
        if (movement == 2U) state = PLAYER_FINISHED;
        else gap_left = 1500U;
    }
}

PlayerState Player_State(void) { return state; }
uint8_t Player_Movement(void) { return movement; }
uint32_t Player_Position(void) { return position; }

/* Runs the production C player with mock buzzer outputs on a host PC. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "player.h"
#include "buzzer.h"
#include "generated/score_data.h"

static uint8_t pins[8];
static int trace;
static uint32_t wall;
void Buzzer_Init(void) {}
void Buzzer_Set(uint8_t voice, uint8_t note)
{
    assert(voice < 8 && note <= 128);
    pins[voice] = note;
    if (trace) printf("%u,%u,%u\n", wall, voice, note);
}
void Buzzer_Silence(void) { for (int i = 0; i < 8; ++i) Buzzer_Set(i, 0); }
static void silent(void) { for (int i = 0; i < 8; ++i) assert(!pins[i]); }

int main(int argc, char **argv)
{
    Player_Init();
    assert(Player_State() == PLAYER_READY);
    silent();
    if (argc == 2) {
        unsigned m = (unsigned)atoi(argv[1]);
        assert(m < 3);
        for (unsigned i = 0; i < m; ++i) Player_Next(0);
        trace = 1;
        Player_Toggle(0);
        for (wall = 1; wall <= scores[m].duration_ms; ++wall) Player_Update(wall);
        return 0;
    }
    /* Pausing holds score time and physically mutes every voice. */
    Player_Toggle(100);
    Player_Update(2100);
    assert(Player_Position() == 2000);
    Player_Toggle(2100);
    assert(Player_State() == PLAYER_PAUSED);
    silent();
    Player_Update(9000);
    assert(Player_Position() == 2000);
    Player_Toggle(9100);
    Player_Update(9200);
    assert(Player_Position() == 2100);
    /* Selecting next movement silences and rewinds it. */
    Player_Next(9300);
    assert(Player_Movement() == 1 && Player_State() == PLAYER_READY);
    assert(Player_Position() == 0);
    silent();
    /* Wall clock rollover must not advance score by billions of ms. */
    Player_Toggle(UINT32_MAX - 5U);
    Player_Update(4U);
    assert(Player_Position() == 10U);
    Player_Init();
    Player_Toggle(0);
    const uint32_t total = scores[0].duration_ms + scores[1].duration_ms + scores[2].duration_ms + 3000;
    for (wall = 1; wall <= total; ++wall) Player_Update(wall);
    assert(Player_State() == PLAYER_FINISHED && Player_Movement() == 2);
    silent();
    Player_Toggle(total + 1);
    assert(Player_State() == PLAYER_PLAYING && Player_Movement() == 0 && Player_Position() == 0);
    puts("C player: pause/resume, select, rollover, all movements and replay PASS");
    return 0;
}

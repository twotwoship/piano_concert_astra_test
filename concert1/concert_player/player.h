#ifndef PLAYER_H
#define PLAYER_H
#include <stdint.h>
typedef enum { PLAYER_READY, PLAYER_PLAYING, PLAYER_PAUSED, PLAYER_FINISHED, PLAYER_ERROR } PlayerState;
void Player_Init(void);
void Player_Toggle(uint32_t now);
void Player_Next(uint32_t now);
void Player_Update(uint32_t now);
PlayerState Player_State(void);
uint8_t Player_Movement(void); /* 0..2 */
uint32_t Player_Position(void);
#endif

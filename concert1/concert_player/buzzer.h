#ifndef BUZZER_H
#define BUZZER_H
#include <stdint.h>
void Buzzer_Init(void);
/* voice: 0..7, note_plus_one: 0=silent, 1..128=MIDI pitch+1 */
void Buzzer_Set(uint8_t voice, uint8_t note_plus_one);
void Buzzer_Silence(void);
#endif

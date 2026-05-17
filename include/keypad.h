#ifndef KEYPAD_H
#define KEYPAD_H

#include <stdint.h>

void keypad_init_pins();
void keypad_init_timer();
uint8_t keypad_read_rows();


#endif

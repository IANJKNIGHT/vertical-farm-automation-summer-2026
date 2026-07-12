#include "pico/stdlib.h"
#include <hardware/gpio.h>
#include <stdio.h>
#include "queue.h"
#include "hardware/timer.h"

// Global column variable
int col = -1;
char key = '\0';

// Global key state
static bool state[16]; // Are keys pressed/released

// Keymap for the keypad
const char keymap[17] = "DCBA#9630852*741";

void keypad_drive_column();
void keypad_isr();
int drive_col_unused_alarm_id = -1;
int read_row_unused_alarm_id = -1;

void keypad_init_pins();
void keypad_init_timer();
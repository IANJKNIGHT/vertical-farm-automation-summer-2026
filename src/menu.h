#ifndef MENU_H
#define MENU_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_NODES 4 // Adjust this to your desired N number of nodes

// Button Input Enum
enum ButtonInput
{
    BTN_NONE,
    BTN_UP,    // represented by 1
    BTN_DOWN,  // represented by 2
    BTN_LEFT,  // represented by 3
    BTN_RIGHT, // represented by 4
    BTN_ENTER, // represented by 5
    BTN_BACK   // represented by 6
};

// Menu States
typedef enum
{
    STATE_ROOT,
    STATE_NODE_OPT,
    STATE_FAN_SELECT,
    STATE_FAN_ADJUST_TYPE,
    STATE_LARGE_ADJUST,
    STATE_SMALL_ADJUST,
    STATE_SERVO_CTRL,
    STATE_GENERAL_INFO
} MenuState;

// External placeholders for your sensor/hardware variables
extern uint16_t current_rpm;
extern float temp_delta;
extern float humidity_delta;

// Functions
void menu_init(void);
void menu_update(enum ButtonInput current_button_press);
void menu_render(void);
enum ButtonInput get_button_input(char key_char);

#endif // MENU_H
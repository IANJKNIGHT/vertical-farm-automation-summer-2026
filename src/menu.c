#include "menu.h"
#include "tft_display.h"
#include <stdio.h>

// Initialize global variables (Update these values in your main loop from your sensors/potentiometer)
uint16_t current_rpm = 1500;
float temp_delta = 0.0;
float humidity_delta = 0.0;
extern enum ButtonInput current_button_press;

static MenuState current_state = STATE_ROOT;
static int8_t current_selection = 0;
static bool node_selected[MAX_NODES] = {false};
static bool force_redraw = true;

// Utility function to cleanly paint menu text rows
static void draw_menu_item(uint16_t x, uint16_t y, const char *text, bool is_selected)
{
    uint16_t fg = is_selected ? COLOR_BLACK : COLOR_WHITE;
    uint16_t bg = is_selected ? COLOR_WHITE : COLOR_BLACK;
    tft_print(x, y, "                      ", bg, bg, 2); // clear text line
    tft_print(x, y, text, fg, bg, 2);
}

void menu_init(void)
{
    tft_fill_screen(COLOR_BLACK);
    current_state = STATE_ROOT;
    current_selection = 0;
    force_redraw = true;
}

void menu_update(void)
{
    if (current_button_press == BTN_NONE)
        return;

    MenuState prev_state = current_state;

    switch (current_state)
    {
    case STATE_ROOT:
        if (current_button_press == BTN_UP || current_button_press == BTN_DOWN)
            current_selection = (current_selection == 0) ? 1 : 0;
        if (current_button_press == BTN_ENTER)
        {
            current_state = (current_selection == 0) ? STATE_NODE_OPT : STATE_GENERAL_INFO;
            current_selection = 0;
        }
        break;

    case STATE_NODE_OPT:
        if (current_button_press == BTN_UP || current_button_press == BTN_DOWN)
            current_selection = (current_selection == 0) ? 1 : 0;
        if (current_button_press == BTN_BACK)
        {
            current_state = STATE_ROOT;
            current_selection = 0;
        }
        if (current_button_press == BTN_ENTER)
        {
            current_state = (current_selection == 0) ? STATE_FAN_SELECT : STATE_SERVO_CTRL;
            current_selection = 0;
        }
        break;

    case STATE_FAN_SELECT:
        if (current_button_press == BTN_UP)
        {
            current_selection = (current_selection == 0) ? MAX_NODES : current_selection - 1;
        }
        if (current_button_press == BTN_DOWN)
        {
            current_selection = (current_selection == MAX_NODES) ? 0 : current_selection + 1;
        }
        if (current_button_press == BTN_ENTER)
        {
            if (current_selection < MAX_NODES)
            {
                node_selected[current_selection] = !node_selected[current_selection]; // Toggle multiselect
            }
            else
            {
                current_state = STATE_FAN_ADJUST_TYPE; // "PROCEED" selected
                current_selection = 0;
            }
        }
        if (current_button_press == BTN_BACK)
        {
            current_state = STATE_NODE_OPT;
            current_selection = 0;
        }
        break;

    case STATE_FAN_ADJUST_TYPE:
        if (current_button_press == BTN_UP || current_button_press == BTN_DOWN)
            current_selection = (current_selection == 0) ? 1 : 0;
        if (current_button_press == BTN_BACK)
        {
            current_state = STATE_FAN_SELECT;
            current_selection = 0;
        }
        if (current_button_press == BTN_ENTER)
        {
            current_state = (current_selection == 0) ? STATE_LARGE_ADJUST : STATE_SMALL_ADJUST;
            current_selection = 0;
        }
        break;

    case STATE_LARGE_ADJUST:
        // --- PLACEHOLDER FOR POTENTIOMETER READ LOGIC ---
        // Inside your main loop, read your potentiometer value and update `current_rpm`
        if (current_button_press == BTN_BACK)
        {
            current_state = STATE_FAN_ADJUST_TYPE;
            current_selection = 0;
        }
        break;

    case STATE_SMALL_ADJUST:
        // --- PLACEHOLDER FOR MANUAL STEPPING LOGIC ---
        if (current_button_press == BTN_UP)
            current_rpm += 50;
        if (current_button_press == BTN_DOWN)
            if (current_rpm >= 50)
                current_rpm -= 50;

        if (current_button_press == BTN_BACK)
        {
            current_state = STATE_FAN_ADJUST_TYPE;
            current_selection = 0;
        }
        break;

    case STATE_SERVO_CTRL:
        // --- PLACEHOLDER FOR SERVO DRIVE LOGIC ---
        // Use btn == BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT here to position your servo
        if (current_button_press == BTN_BACK)
        {
            current_state = STATE_NODE_OPT;
            current_selection = 1; // Highlight Servo row on return
        }
        break;

    case STATE_GENERAL_INFO:
        if (current_button_press == BTN_BACK)
        {
            current_state = STATE_ROOT;
            current_selection = 1; // Highlight Gen Info row on return
        }
        break;
    }

    // Only completely wipe display if changing screens entirely
    if (current_state != prev_state)
    {
        tft_fill_screen(COLOR_BLACK);
    }
    force_redraw = true;
}

void menu_render(void)
{
    if (!force_redraw)
        return;
    force_redraw = false;

    char buf[32];

    switch (current_state)
    {
    case STATE_ROOT:
        tft_print(10, 10, "-- MAIN MENU --", COLOR_BLUE, COLOR_BLACK, 2);
        draw_menu_item(20, 50, "NODE", (current_selection == 0));
        draw_menu_item(20, 80, "GENERAL INFO", (current_selection == 1));
        break;

    case STATE_NODE_OPT:
        tft_print(10, 10, "-- NODE OPTIONS --", COLOR_BLUE, COLOR_BLACK, 2);
        draw_menu_item(20, 50, "FAN RPM", (current_selection == 0));
        draw_menu_item(20, 80, "SERVO CTRL", (current_selection == 1));
        break;

    case STATE_FAN_SELECT:
        tft_print(10, 10, "SELECT NODES:", COLOR_BLUE, COLOR_BLACK, 2);
        for (int i = 0; i < MAX_NODES; i++)
        {
            snprintf(buf, sizeof(buf), "[%c] NODE %d", node_selected[i] ? 'X' : ' ', i + 1);
            draw_menu_item(20, 45 + (i * 25), buf, (current_selection == i));
        }
        draw_menu_item(20, 45 + (MAX_NODES * 25), "-> PROCEED", (current_selection == MAX_NODES));
        break;

    case STATE_FAN_ADJUST_TYPE:
        tft_print(10, 10, "-- FAN ADJUST --", COLOR_BLUE, COLOR_BLACK, 2);
        draw_menu_item(20, 50, "LARGE ADJUST", (current_selection == 0));
        draw_menu_item(20, 80, "SMALL ADJUST", (current_selection == 1));
        break;

    case STATE_LARGE_ADJUST:
        tft_print(10, 10, "MODE: LARGE ADJUST", COLOR_RED, COLOR_BLACK, 2);
        tft_print(10, 50, "TURN POTENTIOMETER", COLOR_WHITE, COLOR_BLACK, 1);
        tft_print(10, 70, "PRESS BACK WHEN FINISHED", COLOR_WHITE, COLOR_BLACK, 1);

        snprintf(buf, sizeof(buf), "CURRENT RPM: %d", current_rpm);
        tft_print(10, 110, buf, COLOR_GREEN, COLOR_BLACK, 2);
        break;

    case STATE_SMALL_ADJUST:
        tft_print(10, 10, "MODE: SMALL ADJUST", COLOR_RED, COLOR_BLACK, 2);
        tft_print(10, 50, "PRESS UP OR DOWN", COLOR_WHITE, COLOR_BLACK, 1);
        tft_print(10, 70, "PRESS BACK WHEN FINISHED", COLOR_WHITE, COLOR_BLACK, 1);

        snprintf(buf, sizeof(buf), "CURRENT RPM: %d", current_rpm);
        tft_print(10, 110, buf, COLOR_GREEN, COLOR_BLACK, 2);
        break;

    case STATE_SERVO_CTRL:
        tft_print(10, 10, "MODE: SERVO CTRL", COLOR_RED, COLOR_BLACK, 2);
        tft_print(10, 50, "PRESS UP, DOWN, LEFT, RIGHT", COLOR_WHITE, COLOR_BLACK, 1);
        tft_print(10, 70, "PRESS BACK WHEN FINISHED", COLOR_WHITE, COLOR_BLACK, 1);
        break;

    case STATE_GENERAL_INFO:
        tft_print(10, 10, "-- GENERAL INFO --", COLOR_BLUE, COLOR_BLACK, 2);

        snprintf(buf, sizeof(buf), "TEMP DELTA: %.1f C", temp_delta);
        tft_print(20, 60, buf, COLOR_WHITE, COLOR_BLACK, 2);

        snprintf(buf, sizeof(buf), "HUMIDITY DELTA: %.1f %%", humidity_delta);
        tft_print(20, 90, buf, COLOR_WHITE, COLOR_BLACK, 2);
        break;
    }
}

enum ButtonInput get_button_input(char key_char)
{
    switch (key_char)
    {
    case '1':
        return BTN_UP;
    case '2':
        return BTN_DOWN;
    case '3':
        return BTN_LEFT;
    case '4':
        return BTN_RIGHT;
    case '5':
        return BTN_ENTER;
    case '6':
        return BTN_BACK;
    default:
        return BTN_NONE;
    }
}
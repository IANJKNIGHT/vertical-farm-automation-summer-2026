#include "queue.h"
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/irq.h"

extern char key_char;
extern void turn_on_selected_leds(char key);
extern char last_num_entered;
// Remove 'extern' and add 'volatile'
volatile uint8_t blink_step = 0; // Tracks our current position in time

enum SystemTimerState
{
    MODE_DEFAULT,
    MODE_MANUAL_ADJUST,
    ENTER_DAYS,
    ENTER_HOURS,
    ENTER_MINUTES,
    ENTER_SECONDS,
    MODE_TIMED_RUN
};

extern enum SystemTimerState system_timer_state;

enum EnterState
{
    NO_ENTRY,
    FIRST_PRESS,
    SAVE_STATE_1,
    SECOND_PRESS,
    SAVE_STATE_2
};

extern enum EnterState enter_state;

extern int remaining_days;
extern int remaining_hours;
extern int remaining_minutes;
extern int remaining_seconds_seconds;
void start_new_blink_sequence();

void no_entry_back_state()
{
    enter_state = NO_ENTRY;
    switch (system_timer_state)
    {
    case ENTER_DAYS:
        system_timer_state = MODE_MANUAL_ADJUST;
        break;
    case ENTER_HOURS:
        system_timer_state = ENTER_DAYS;
        break;
    case ENTER_MINUTES:
        system_timer_state = ENTER_HOURS;
        break;
    case ENTER_SECONDS:
        system_timer_state = ENTER_MINUTES;
        break;
    case MODE_TIMED_RUN:
        system_timer_state = ENTER_SECONDS;
        break;
    }
}

void noEntryLogic(char stable_key)
{
    switch (stable_key)
    {
    case '0' ... '9':
        enter_state = FIRST_PRESS;
        start_new_blink_sequence();
        last_num_entered = stable_key;
        break;
    case 'B':
        no_entry_back_state();
        break;
    default:
        break;
    }
}

void firstPressLogic(char stable_key)
{
    switch (stable_key)
    {
    case '*':
        enter_state = SAVE_STATE_1;
        break;
    case 'B':
        enter_state = NO_ENTRY;
        break;
    default:
        break;
    }
}

void time_type_saved_save_state_1(char stable_key)
{
    int remaining_time = 10 * (stable_key - '0');
    // FIX: Switch on system_timer_state, not enter_state
    switch (system_timer_state)
    {
        case ENTER_DAYS:
            remaining_days = remaining_time;
            break;
        case ENTER_HOURS:
            remaining_hours = remaining_time;
            break;
        case ENTER_MINUTES:
            remaining_minutes = remaining_time;
            break;
        case ENTER_SECONDS:
            remaining_seconds_seconds = remaining_time;
            break;
        default:
            break;
    }
}

void save_state_1_logic(char stable_key)
{
    switch (stable_key)
    {
    case '0' ... '9':
        enter_state = SECOND_PRESS;
        time_type_saved_save_state_1(stable_key);
        start_new_blink_sequence();
        last_num_entered = stable_key;
        break;
    case 'B':
        enter_state = NO_ENTRY;
        break;
    default:
        break;
    }
}

void secondPressLogic(char stable_key)
{
    switch (stable_key)
    {
    case '*':
        enter_state = SAVE_STATE_2;
        break;
    case 'B':
        enter_state = SAVE_STATE_1;
        break;
    default:
        break;
    }
}

int less_than_59_check(int remaining_time)
{
    return (remaining_time > 59) ? 0 : remaining_time;
}

void time_type_saved_save_state_2(char stable_key)
{
    // FIX: This is the ones digit, do not multiply by 10
    int remaining_time = (stable_key - '0');

    // FIX: Switch on system_timer_state, not enter_state
    switch (system_timer_state)
    {
    case ENTER_DAYS:
        remaining_days += remaining_time;
        break;
    case ENTER_HOURS:
        remaining_hours += remaining_time;
        if (remaining_hours > 23)
            remaining_hours = 0;
        break;
    case ENTER_MINUTES:
        remaining_minutes += remaining_time;
        remaining_minutes = less_than_59_check(remaining_minutes);
        break;
    case ENTER_SECONDS:
        remaining_seconds_seconds += remaining_time;
        remaining_seconds_seconds = less_than_59_check(remaining_seconds_seconds);
        break;
    default:
        break;
    }
}

void save2_next_systemTimerState()
{
    switch (system_timer_state)
    {
    case ENTER_DAYS:
        system_timer_state = ENTER_HOURS;
        break;
    case ENTER_HOURS:
        system_timer_state = ENTER_MINUTES;
        break;
    case ENTER_MINUTES:
        system_timer_state = ENTER_SECONDS;
        break;
    case ENTER_SECONDS:
        system_timer_state = MODE_TIMED_RUN;
        break;
    default:
        break;
    }
}

void save_state_2_logic(char stable_key)
{
    enter_state = NO_ENTRY;
    switch (stable_key)
    {
    case 'D':
        time_type_saved_save_state_2(stable_key); // 1. Save the captured value first
        save2_next_systemTimerState();            // 2. Then transition states safely
        break;
    case 'B':
        break;
    default:
        enter_state = SAVE_STATE_2;
        break;
    }
}

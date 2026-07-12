#include "queue.h"
#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/irq.h"

extern char key_char;
extern void turn_on_selected_leds(char key);
extern char last_num_entered;
// Remove 'extern' and add 'volatile'
volatile uint8_t blink_step = 0; // Tracks our current position in time
extern bool time_val_for_fan_displayed;
extern bool first_num_set;
extern absolute_time_t mode_switch_time_remaining_to_set;

enum SystemTimerState
{
    MODE_DEFAULT,
    MODE_PID_TUNING,
    MODE_MANUAL_ADJUST,
    SEL_TIME_VALS,
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
    SAVE_STATE_2,
    TOO_BIG
};

extern enum EnterState enter_state;

extern int remaining_days;
extern int remaining_hours;
extern int remaining_minutes;
extern int remaining_seconds_seconds;
extern bool entry_too_large_error; // when the entered time is greater than 23 hours or 59 min/seconds
extern bool time_interval_head_set;
extern bool linked_list_set;
struct timerInterval
{
    enum SystemTimerState timer_state;
    struct timerInterval *next;
    struct timerInterval *prev;
};
extern struct timerInterval *timer_interval_head;
struct timerInterval *next;
char possible_inputs_array[5] = {'A', 'B', 'C', 'D'};
enum SystemTimerState possible_states_arr[4] = {ENTER_DAYS, ENTER_HOURS, ENTER_MINUTES, ENTER_SECONDS};
extern int timer_interval_index;
int set_idx = 4;
void start_new_blink_sequence();
void noEntryLogic(char stable_key);
void firstPressLogic(char stable_key);
void save_state_1_logic(char stable_key);
void secondPressLogic(char stable_key);
void save_state_2_logic();
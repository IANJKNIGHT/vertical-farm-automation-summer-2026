#include "queue.h"
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/irq.h"

#define enter_days_led 22
#define enter_hours_led 23
#define enter_minutes_led 24
#define enter_seconds_led 25

enum EnterState
{
    NO_ENTRY,
    FIRST_PRESS,
    SAVE_STATE_1,
    SECOND_PRESS,
    SAVE_STATE_2,
    TOO_BIG
};

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
int col_gpio_trigger_for_manual_mode = 6; // GPIO pin that triggers manual mode entry
int col_gpio_trigger_for_enter_time = 7;  // GPIO pin that triggers timed run entry
extern enum EnterState enter_state;
extern char last_num_entered;
extern int remaining_days;
extern int remaining_hours;
extern int remaining_minutes;
extern int remaining_seconds_seconds;
extern bool time_val_for_fan_displayed;
extern char key_char;
extern uint8_t blink_step; // Globally visible to track our current position in time
extern uint32_t remaining_seconds;
static int state_machine_alarm_num = -1;
extern int key_event_count;
volatile bool blink_phase_toggle = false;
extern bool entry_too_large_error;

void init_state_machine_led();
void init_timer_subsystem(); // Starts up the Alarm 0 infrastructure safely
void start_new_blink_sequence();

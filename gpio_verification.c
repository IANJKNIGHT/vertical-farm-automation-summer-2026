#include "queue.h"
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/irq.h"

#define ALARM_NUM0 0

enum EnterState
{
    NO_ENTRY,
    FIRST_PRESS,
    SAVE_STATE_1,
    SECOND_PRESS,
    SAVE_STATE_2
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
extern char key_char;
extern uint32_t remaining_seconds;
static int state_machine_alarm_num = -1;
volatile bool blink_phase_toggle = false; // Simple atomic heartbeat token for the main loop
// extern int blink_step;

// Helper function to turn on the custom LED pattern based on the selected key
static void turn_on_selected_leds(char key)
{
    switch (key)
    {
    case '0':
        sio_hw->gpio_set = (1 << 23) | (1 << 25);
        break;
    case '1':
        sio_hw->gpio_set = (1 << 22);
        break;
    case '2':
        sio_hw->gpio_set = (1 << 23);
        break;
    case '3':
        sio_hw->gpio_set = (1 << 23) | (1 << 22);
        break;
    case '4':
        sio_hw->gpio_set = (1 << 24);
        break;
    case '5':
        sio_hw->gpio_set = (1 << 24) | (1 << 22);
        break;
    case '6':
        sio_hw->gpio_set = (1 << 24) | (1 << 23);
        break;
    case '7':
        sio_hw->gpio_set = (1 << 24) | (1 << 23) | (1 << 22);
        break;
    case '8':
        sio_hw->gpio_set = (1 << 25);
        break;
    case '9':
        sio_hw->gpio_set = (1 << 25) | (1 << 22);
        break;
    default:
        sio_hw->gpio_set = (1 << 22) | (1 << 23) | (1 << 24) | (1 << 25);
        break;
    }
}

// Helper function to instantly clear all indication LEDs
static void turn_off_all_leds()
{
    sio_hw->gpio_clr = (1 << 22) | (1 << 23) | (1 << 24) | (1 << 25);
}

void update_ui_leds_from_main(char stable_key)
{
    // Clear display registers cleanly
    sio_hw->gpio_clr = (1 << 22) | (1 << 23) | (1 << 24) | (1 << 25);

    // If we're off during a blink phase cycle, leave them off
    if (!blink_phase_toggle && (enter_state == FIRST_PRESS || enter_state == SECOND_PRESS)) {
        return;
    }

    switch (stable_key)
    {
        case '0': sio_hw->gpio_set = (1 << 23) | (1 << 25); break;
        case '1': sio_hw->gpio_set = (1 << 22); break;
        case '2': sio_hw->gpio_set = (1 << 23); break;
        case '3': sio_hw->gpio_set = (1 << 23) | (1 << 22); break;
        case '4': sio_hw->gpio_set = (1 << 24); break;
        case '5': sio_hw->gpio_set = (1 << 24) | (1 << 22); break;
        case '6': sio_hw->gpio_set = (1 << 24) | (1 << 23); break;
        case '7': sio_hw->gpio_set = (1 << 24) | (1 << 23) | (1 << 22); break;
        case '8': sio_hw->gpio_set = (1 << 25); break;
        case '9': sio_hw->gpio_set = (1 << 25) | (1 << 22); break;
        default:  sio_hw->gpio_set = (1 << 22) | (1 << 23) | (1 << 24) | (1 << 25); break;
    }
}

void multi_pattern_timer_handler()
{
    timer_hw->intr = 1u << state_machine_alarm_num;
    uint32_t delay_us = 1000000;

    // Alternate the toggle flag
    blink_phase_toggle = !blink_phase_toggle;

    // Isolate timing patterns out of hardware calculations
    if (enter_state == SECOND_PRESS)
    {
        delay_us = 150000; // Fast pacing tracking
    }
    else
    {
        delay_us = 1000000; // Slow fallback pacing tracking
    }

    timer_hw->alarm[state_machine_alarm_num] = timer_hw->timerawl + delay_us;
}

// Run this function ONCE inside your main initialization code
void init_timer_subsystem()
{
    // Clear any pending interrupt flags on Alarm 0 safely
    state_machine_alarm_num = hardware_alarm_claim_unused(true); // Dynamically claim an unused alarm

    uint irq_num = TIMER0_IRQ_0 + state_machine_alarm_num; // Calculate the corresponding IRQ number

    // Unmask the specific alarm interrupt in the hardware registers directly
    hw_set_bits(&timer_hw->inte, 1u << state_machine_alarm_num);

    // Register the exclusive handler for this alarm's IRQ number
    irq_set_exclusive_handler(irq_num, multi_pattern_timer_handler);
    irq_set_enabled(irq_num, true);
}

// Call this inside set_time_handler() whenever a valid numeric key is accepted
void start_new_blink_sequence()
{
    // Schedule an immediate interrupt execution to start the selected pattern cycle
    timer_hw->alarm[state_machine_alarm_num] = timer_hw->timerawl + 50;
}

void state_machine_handler(uint gpio, uint32_t events)
{
    // Cleanly isolate to only fire when our edge pattern triggers
    if (!(events & GPIO_IRQ_EDGE_RISE))
        return;

    if (gpio == col_gpio_trigger_for_manual_mode)
    {
        if (system_timer_state == MODE_MANUAL_ADJUST)
        {
            if (key_char == 'A')
            {
                sio_hw->gpio_set = ((1 << 22) | (1 << 23) | (1 << 24) | (1 << 25));
            }
        }
    }
    else if (gpio == col_gpio_trigger_for_enter_time)
    {
        if (key_char == 'D')
        {
            switch (system_timer_state)
            {
            case ENTER_DAYS:
                sio_hw->gpio_set = (1 << 22);
                break;
            case ENTER_HOURS:
                sio_hw->gpio_set = (1 << 23);
                break;
            case ENTER_MINUTES:
                sio_hw->gpio_set = (1 << 24);
                break;
            case ENTER_SECONDS:
                sio_hw->gpio_set = (1 << 25);
                break;
            default:
                break;
            }
        }
    }
}

int enter_timed_run_first_state()
{
    switch (key_char)
    {
    case '0' ... '9':
        remaining_seconds = 10 * (key_char - '0');
        enter_state = FIRST_PRESS;
        start_new_blink_sequence(); // 3. Force the timer to instantly adapt to the new pattern
        break;
    default:
        enter_state = NO_ENTRY;
        break;
    }
    return remaining_seconds;
}

int enter_timed_run_second_state() // fill in
{

    switch (key_char)
    {
    case '0' ... '9':
        remaining_seconds += (key_char - '0');
        enter_state = SECOND_PRESS;
        break;
    case 'C':
        // Clear the entry
        remaining_seconds = 0;
        enter_state = FIRST_PRESS;
        break;
    default:
        enter_state = SECOND_PRESS;
    }

    if (enter_state == ENTER_HOURS && remaining_seconds > 23)
    {
        // printf("Invalid hours input. Please enter a value between 0 and 23.\n");
        remaining_seconds = 0;
        enter_state = FIRST_PRESS;
    }
    else if ((enter_state == ENTER_MINUTES || enter_state == ENTER_SECONDS) && remaining_seconds > 59)
    {
        remaining_seconds = 0;
        enter_state = FIRST_PRESS;
    }
    else
    {
        start_new_blink_sequence(); // 3. Force the timer to instantly adapt to the new pattern
    }

    return remaining_seconds;
}
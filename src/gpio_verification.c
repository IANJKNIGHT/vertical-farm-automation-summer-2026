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

// Helper function to turn on the custom LED pattern based on the selected key
void turn_on_selected_leds(char key)
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

void multi_pattern_timer_handler()
{
    // 1. Acknowledge the hardware timer interrupt immediately
    timer_hw->intr = 1u << state_machine_alarm_num;

    // 2. Advance the time step (0 to 15 wraps around nicely for pattern matchers)
    blink_step = (blink_step) % 8;
    blink_step += 1;

    // 3. Keep a fixed cadence of 150ms (150,000 microseconds)
    uint32_t delay_us = 250000;
    timer_hw->alarm[state_machine_alarm_num] = timer_hw->timerawl + delay_us;
}

void display_time_input(int total_time)
{
    switch (total_time)
    {
    case 0:
        break;
    case 1:
        sio_hw->gpio_set = (1 << 22);
        break;
    case 2:
        sio_hw->gpio_set = (1 << 23);
        break;
    case 3:
        sio_hw->gpio_set = (1 << 23) | (1 << 22);
        break;
    case 4:
        sio_hw->gpio_set = (1 << 24);
        break;
    case 5:
        sio_hw->gpio_set = (1 << 24) | (1 << 22);
        break;
    case 6:
        sio_hw->gpio_set = (1 << 24) | (1 << 23);
        break;
    case 7:
        sio_hw->gpio_set = (1 << 24) | (1 << 23) | (1 << 22);
        break;
    case 8:
        sio_hw->gpio_set = (1 << 25);
        break;
    case 9:
        sio_hw->gpio_set = (1 << 25) | (1 << 22);
        break;
    case 10:
        sio_hw->gpio_set = (1 << 25) | (1 << 23);
        break;
    case 11:
        sio_hw->gpio_set = (1 << 25) | (1 << 23) | (1 << 22);
        break;
    case 12:
        sio_hw->gpio_set = (1 << 25) | (1 << 24);
        break;
    case 13:
        sio_hw->gpio_set = (1 << 25) | (1 << 24) | (1 << 22);
        break;
    case 14:
        sio_hw->gpio_set = (1 << 25) | (1 << 24) | (1 << 23);
        break;
    default:
        sio_hw->gpio_set = (1 << 25) | (1 << 24) | (1 << 23) | (1 << 22);
        break;
    }
}

void blink_time_frame()
{
    switch (system_timer_state)
    {
    case ENTER_DAYS:
        sio_hw->gpio_set = (1 << enter_days_led);
        break;
    case ENTER_HOURS:
        sio_hw->gpio_set = (1 << enter_hours_led);
        break;
    case ENTER_MINUTES:
        sio_hw->gpio_set = (1 << enter_minutes_led);
        break;
    case ENTER_SECONDS:
        sio_hw->gpio_set = (1 << enter_seconds_led);
        break;
    case MODE_DEFAULT:
    case MODE_MANUAL_ADJUST:
        break;
    }
}

// Low-level GPIO driving function called exclusively inside the main loop
void update_ui_leds_from_main(char stable_key)
{
    // Always start by clearing down all output bits to prevent visual artifacts
    turn_off_all_leds();

    bool should_be_on = false;
    if (system_timer_state == MODE_MANUAL_ADJUST)
    {
        sio_hw->gpio_set = 15 << 22;
    }
    else if (entry_too_large_error)
    {
        switch (blink_step - 1)
        {
        case 0:
            sio_hw->gpio_set = (1 << enter_seconds_led);
            break;
        case 1:
            sio_hw->gpio_set = (1 << enter_minutes_led);
            break;
        case 2:
            sio_hw->gpio_set = (1 << enter_hours_led);
            break;
        case 3:
            sio_hw->gpio_set = (1 << enter_days_led);
            break;
        case 4:
        case 6:
            break;
        case 5:
            blink_time_frame();
            break;
        case 7:
            blink_time_frame();
            entry_too_large_error = false;
            break;
        }

        enter_state = NO_ENTRY;
    }
    else if (system_timer_state == MODE_TIMED_RUN)
    {
        if (time_val_for_fan_displayed)
        {
            switch (blink_step)
            {
            case 0:
            case 1:
                if (remaining_days <= 15 || blink_step == 0)
                {
                    display_time_input(remaining_days);
                }
                break;
            case 2:
            case 3:
                if (remaining_hours <= 15 || blink_step == 2)
                {
                    display_time_input(remaining_hours);
                }
                break;
            case 4:
            case 5:
                if (remaining_minutes <= 15 || blink_step == 4)
                {
                    display_time_input(remaining_minutes);
                }
                break;
            case 6:
            case 7:
                if (remaining_seconds_seconds <= 15 || blink_step == 0)
                {
                    display_time_input(remaining_seconds_seconds);
                }
                break;
            }
            time_val_for_fan_displayed = true;
            if (blink_step == 7)
            {
                time_val_for_fan_displayed = false;
            }
        }
    }
    else
    {
        switch (enter_state)
        {
        case SAVE_STATE_1:
        case SAVE_STATE_2:
            // Mode 1: Hold the value solid ON continuously
            should_be_on = true;
            break;

        case FIRST_PRESS:
            // Mode 2: Single Blink Pattern (150ms ON, then a long pause)
            // A full cycle takes 8 steps (~1.2 seconds)
            // Step 0: ON. Steps 1 to 7: OFF.
            if ((blink_step % 8) == 0)
            {
                should_be_on = true;
            }
            break;
        case SECOND_PRESS:
            // Mode 3: Rapid Double-Blink Burst
            // Step 0: ON (150ms)
            // Step 1: OFF (150ms)
            // Step 2: ON (150ms)
            // Steps 3 to 9: OFF (1.05s long trailing sync pause)
            // Total cycle = 10 steps (~1.5 seconds)
            if (blink_step == 1 || blink_step == 3)
            {
                should_be_on = true;
            }
            break;

        case NO_ENTRY:
            switch (system_timer_state)
            {
            case ENTER_DAYS:
                sio_hw->gpio_set = 1 << 22;
                break;
            case ENTER_HOURS:
                sio_hw->gpio_set = 1 << 23;
                break;
            case ENTER_MINUTES:
                sio_hw->gpio_set = 1 << 24;
                break;
            case ENTER_SECONDS:
                sio_hw->gpio_set = 1 << 25;
                break;
            default:
                break;
            }
        default:
            // Safety state / idle
            should_be_on = false;
            break;
        }
    }

    // Drive the hardware registers based on the pattern map output
    if (should_be_on)
    {
        char read_char = (enter_state == SAVE_STATE_1) || (enter_state == SAVE_STATE_2) ? last_num_entered : stable_key;
        switch (read_char)
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
    // Force the clock counter back to 0 so the first cycle begins ON immediately
    blink_step = 0;
    // Force an immediate timer interrupt to fire in 50 microseconds
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
            sio_hw->gpio_set = ((1 << 22) | (1 << 23) | (1 << 24) | (1 << 25));
        }
    }
    else if (gpio == col_gpio_trigger_for_enter_time)
    {
        if (key_char == '#')
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

void init_state_machine_led()
{
    // Initialize GPIO 22-25 as SIO outputs for the LEDs
    for (int gpio = 22; gpio <= 25; gpio++)
    {
        uint32_t mask = 1u << (gpio & 0x1fu);
        sio_hw->gpio_oe_set = mask;
        sio_hw->gpio_clr = mask;

        hw_write_masked(&pads_bank0_hw->io[gpio],
                        PADS_BANK0_GPIO0_IE_BITS,
                        PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO0_OD_BITS);
        io_bank0_hw->io[gpio].ctrl = GPIO_FUNC_SIO << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;
        hw_clear_bits(&pads_bank0_hw->io[gpio], PADS_BANK0_GPIO0_ISO_BITS);
    }

    uint32_t pin_mask = (1u << col_gpio_trigger_for_manual_mode) | (1u << col_gpio_trigger_for_enter_time);

    // 1. Assign the callback using raw handling methods safely
    gpio_set_irq_callback(&state_machine_handler);

    // 2. Clear any pending legacy interrupts sitting in the registers
    gpio_acknowledge_irq(col_gpio_trigger_for_manual_mode, pin_mask);
    gpio_acknowledge_irq(col_gpio_trigger_for_enter_time, pin_mask);

    // 3. --- CRITICAL REGISTRATION FIX ---
    // Instead of using gpio_set_irq_enabled (which calls irq_set_exclusive_handler behind the scenes),
    // set the exact edge-rising mask bits directly into the hardware configuration banks.
    //
    // GPIO 6 Rising Edge = Bit 24 of Procurement Register 0
    // GPIO 7 Rising Edge = Bit 28 of Procurement Register 0
    // hw_set_bits(&io_bank0_hw->proc0_irq_ctrl.inte[0], (1u << 24) | (1u << 28));
    gpio_set_irq_enabled(col_gpio_trigger_for_manual_mode, GPIO_IRQ_EDGE_RISE, true);
    gpio_set_irq_enabled(col_gpio_trigger_for_enter_time, GPIO_IRQ_EDGE_RISE, true);
    gpio_set_dormant_irq_enabled(col_gpio_trigger_for_enter_time, GPIO_IRQ_EDGE_RISE, true); // Enable dormant wake-up on the timed run trigger pin
    // 4. Safe hardware vector release
    irq_set_enabled(IO_IRQ_BANK0, true);
}

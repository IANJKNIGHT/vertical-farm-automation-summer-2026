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
    SECOND_PRESS
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
int col_gpio_trigger_for_enter_time = 7; // GPIO pin that triggers timed run entry
extern enum EnterState enter_state;
extern char key_char;


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
    default:
        sio_hw->gpio_set = (1 << 25) | (1 << 22);
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
    timer_hw->intr = 1u << ALARM_NUM0;

    // Static sequence step counter tracks exactly where we are in a blinking pattern
    static uint8_t blink_step = 0;
    uint32_t delay_us = 1000000; // Safe default fallback delay (1 second)

    // --- MODE 1: FIRST PRESS PATTERN (Steady Balanced Blinker) ---
    if (enter_state == FIRST_PRESS)
    {
        if (blink_step == 0)
        {
            turn_on_selected_leds(key_char);
            delay_us = 1000000; // Hold ON for 1.0 second
            blink_step = 1;     // Advance to off step
        }
        else
        {
            turn_off_all_leds();
            delay_us = 1000000; // Keep OFF for 1.0 second
            blink_step = 0;     // Loop back to start
        }
    }
    // --- MODE 2: SECOND PRESS PATTERN (Rapid Double-Blink Burst) ---
    else if (enter_state == SECOND_PRESS)
    {
        switch (blink_step)
        {
        case 0: // First sharp flash ON
            turn_on_selected_leds(key_char);
            delay_us = 150000; // ON for 150ms
            blink_step = 1;
            break;
        case 1: // First sharp flash OFF
            turn_off_all_leds();
            delay_us = 150000; // OFF for 150ms
            blink_step = 2;
            break;
        case 2: // Second sharp flash ON
            turn_on_selected_leds(key_char);
            delay_us = 150000; // ON for 150ms
            blink_step = 3;
            break;
        case 3: // Long trailing sync pause
        default:
            turn_off_all_leds();
            delay_us = 1000000; // Keep OFF for 1.0 second before restarting burst
            blink_step = 0;     // Reset sequence to step 0
            break;
        }
    }
    // --- MODE 3: SAFETY / NO ENTRY STATE ---
    else
    {
        turn_off_all_leds();
        blink_step = 0;
        return; // Halt alarm rescheduling if no active entry state is ongoing
    }

    // Schedule the next alarm tick cleanly using purely integer variables
    timer_hw->alarm[ALARM_NUM0] = timer_hw->timerawl + delay_us;
}

// Run this function ONCE inside your main initialization code
void init_timer_subsystem() {
    // Clear any pending interrupt flags on Alarm 0 safely
    timer_hw->intr = 1u << ALARM_NUM0;
    
    // Unmask Alarm 0 interrupt output
    hw_set_bits(&timer_hw->inte, 1u << ALARM_NUM0);

    // Lock the exclusive handler into the system vector table
    irq_set_exclusive_handler(TIMER0_IRQ_0, multi_pattern_timer_handler);
    irq_set_enabled(TIMER0_IRQ_0, true);
}

// Call this inside set_time_handler() whenever a valid numeric key is accepted
void start_new_blink_sequence() {
    // Schedule an immediate interrupt execution to start the selected pattern cycle
    timer_hw->alarm[ALARM_NUM0] = timer_hw->timerawl + 50; 
}

void state_machine_handler(uint gpio, uint32_t events)
{
    // Check if the manual mode trigger fired
    if (gpio == col_gpio_trigger_for_manual_mode && (events & GPIO_IRQ_EDGE_RISE))
    {
        if (system_timer_state == MODE_MANUAL_ADJUST)
        {
            uint32_t led_mask = ((1 << 22) | (1 << 23) | (1 << 24) | (1 << 25));
            sio_hw->gpio_set = led_mask;
        }
    }

    // Check if the timed run trigger fired
    if (gpio == col_gpio_trigger_for_enter_time && (events & GPIO_IRQ_EDGE_RISE))
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

    // NOTE: You do NOT need to call gpio_acknowledge_irq() here. 
    // The SDK master handler clears the interrupt flag for you automatically when this callback finishes!
}



void init_state_machine_led()

{

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

    // Set the interrupt handler for GPIO 22-25 to manual_mode_adjust_led_handler

    // Set the interrupt handler for our source trigger pin

    gpio_set_irq_callback(&state_machine_handler);


    // Turn on global IO interrupts and enable the specific pin edge

    irq_set_enabled(IO_IRQ_BANK0, true);

    gpio_set_irq_enabled(col_gpio_trigger_for_manual_mode, GPIO_IRQ_EDGE_RISE, true);

    gpio_set_irq_enabled(col_gpio_trigger_for_enter_time, GPIO_IRQ_EDGE_RISE, true);

}


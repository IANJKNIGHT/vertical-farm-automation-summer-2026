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
extern uint32_t remaining_seconds;
static int state_machine_alarm_num = -1;


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

void multi_pattern_timer_handler()
{
    // 1. Acknowledge the hardware timer interrupt immediately
    timer_hw->intr = 1u << state_machine_alarm_num;

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
    timer_hw->alarm[state_machine_alarm_num] = timer_hw->timerawl + delay_us;
}

// Run this function ONCE inside your main initialization code
void init_timer_subsystem() {
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
void start_new_blink_sequence() {
    // Schedule an immediate interrupt execution to start the selected pattern cycle
    timer_hw->alarm[state_machine_alarm_num] = timer_hw->timerawl + 50; 
}

// void state_machine_handler(uint gpio, uint32_t events)
// {
//     // Check if the manual mode trigger fired
//     uint32_t col_gpio_mask = 1u << col_gpio_trigger_for_manual_mode;
//     if (gpio == col_gpio_trigger_for_manual_mode && (events & GPIO_IRQ_EDGE_RISE))
//     {
//         if (system_timer_state == MODE_MANUAL_ADJUST)
//         {
//             uint32_t led_mask = ((1 << 22) | (1 << 23) | (1 << 24) | (1 << 25));
//             sio_hw->gpio_set = led_mask;
            
//         }
//         gpio_acknowledge_irq(col_gpio_trigger_for_enter_time, col_gpio_mask); 
//     }
//     uint32_t col_enter_time_mask = 1u << col_gpio_trigger_for_enter_time;
//     // Check if the timed run trigger fired
//     if (gpio == col_gpio_trigger_for_enter_time && (events & GPIO_IRQ_EDGE_RISE))
//     {
//         if (key_char == '#')
//         {
//             switch (system_timer_state)
//             {
//             case ENTER_DAYS:
//                 sio_hw->gpio_set = (1 << 22);
//                 break;
//             case ENTER_HOURS:
//                 sio_hw->gpio_set = (1 << 23);
//                 break;
//             case ENTER_MINUTES:
//                 sio_hw->gpio_set = (1 << 24);
//                 break;
//             case ENTER_SECONDS:
//                 sio_hw->gpio_set = (1 << 25);
//                 break;
//             default:
//                 break;
//             }
//         }
//         gpio_acknowledge_irq(col_gpio_trigger_for_enter_time, col_enter_time_mask); 
//     }

//     // NOTE: You do NOT need to call gpio_acknowledge_irq() here. 
//     // The SDK master handler clears the interrupt flag for you automatically when this callback finishes!
// }

void state_machine_handler(uint gpio, uint32_t events)
{
    // Cleanly isolate to only fire when our edge pattern triggers
    if (!(events & GPIO_IRQ_EDGE_RISE)) return;

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
                case ENTER_DAYS:    sio_hw->gpio_set = (1 << 22); break;
                case ENTER_HOURS:   sio_hw->gpio_set = (1 << 23); break;
                case ENTER_MINUTES: sio_hw->gpio_set = (1 << 24); break;
                case ENTER_SECONDS: sio_hw->gpio_set = (1 << 25); break;
                default: break;
            }
        }
    }
}


// void init_state_machine_led()

// {

//     for (int gpio = 22; gpio <= 25; gpio++)

//     {

//         uint32_t mask = 1u << (gpio & 0x1fu);

//         sio_hw->gpio_oe_set = mask;

//         sio_hw->gpio_clr = mask;



//         hw_write_masked(&pads_bank0_hw->io[gpio],

//                         PADS_BANK0_GPIO0_IE_BITS,

//                         PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO0_OD_BITS);

//         io_bank0_hw->io[gpio].ctrl = GPIO_FUNC_SIO << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;

//         hw_clear_bits(&pads_bank0_hw->io[gpio], PADS_BANK0_GPIO0_ISO_BITS);

//     }

//     // Set the interrupt handler for GPIO 22-25 to manual_mode_adjust_led_handler

//     // Set the interrupt handler for our source trigger pin

    


//     // Turn on global IO interrupts and enable the specific pin edge

    

//     uint32_t pin_mask = (1u << col_gpio_trigger_for_manual_mode) | (1u << col_gpio_trigger_for_enter_time);
    
//     gpio_add_raw_irq_handler(pin_mask, state_machine_handler);
//     // Clear any pending interrupts on the trigger pins before enabling
//     gpio_acknowledge_irq(col_gpio_trigger_for_manual_mode, pin_mask);

//     gpio_set_irq_enabled(col_gpio_trigger_for_manual_mode, GPIO_IRQ_EDGE_RISE, true);
//     gpio_set_irq_enabled(col_gpio_trigger_for_enter_time, GPIO_IRQ_EDGE_RISE, true);
//     gpio_set_dormant_irq_enabled(col_gpio_trigger_for_enter_time, GPIO_IRQ_EDGE_RISE, true); // Enable dormant wake-up on the timed run trigger pin
//     // This safely enables edge-rise tracking for both pins across Core 0 without hitting the exclusive lock twice
//     irq_set_enabled(IO_IRQ_BANK0, true);
// }

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

int enter_timed_run_first_state(enum SystemTimerState state, uint16_t key_event) 
{
    if (key_event != 0)
    {
        char local_char = (char)(key_event & 0xFF);
        switch (local_char)
        {
        case '0' ... '9':
            remaining_seconds = 10 * (local_char - '0');
            key_char = local_char;        // 1. Update the shared global char for the LEDs
            enter_state = SECOND_PRESS;   // 2. Advance the state
            start_new_blink_sequence();   // 3. Force the timer to instantly adapt to the new pattern
            break;
        default:
            
            enter_state = FIRST_PRESS;
            break;
        }
    }
    return remaining_seconds;
}

int enter_timed_run_second_state(enum SystemTimerState state, uint16_t key_event) // fill in
{
    if (key_event != 0)
    {
        key_char = (char)(key_event & 0xFF);
        switch (key_char)
        {
        case '0' ... '9':
            remaining_seconds += (key_char - '0');
            break;
        case 'C':
            // Clear the entry
            remaining_seconds = 0;
            enter_state = FIRST_PRESS;
            break;
        default:
            enter_state = SECOND_PRESS;
        }
    }
    if (state == ENTER_HOURS && remaining_seconds > 23)
    {
        // printf("Invalid hours input. Please enter a value between 0 and 23.\n");
        remaining_seconds = 0;
        enter_state = FIRST_PRESS;
    }
    if ((state == ENTER_MINUTES || state == ENTER_SECONDS) && remaining_seconds > 59)
    {
        // printf("Invalid minutes input. Please enter a value between 0 and 59.\n");
        remaining_seconds = 0;
        enter_state = FIRST_PRESS;
    }
    return remaining_seconds;
}

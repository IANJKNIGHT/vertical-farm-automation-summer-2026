#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/irq.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "queue.h"
#include "cmsis_gcc.h"

//////////////////////////////////////////////////////////////////////////////

#define NUM_SAMPLES 1000
#define MASTER_BUFFER_SIZE (NUM_SAMPLES * 2) // 2000 elements total
#define SPEED_SET_TIME 30                    // seconds

// Double-sized master ping-pong buffers
uint16_t master_buffer1[MASTER_BUFFER_SIZE];
uint16_t master_buffer2[MASTER_BUFFER_SIZE];

uint16_t adc_fan_speed_control[NUM_SAMPLES];
uint16_t adc_duty_cycle_result[NUM_SAMPLES];
int second_press_blink_count = 0;

int dma_master_chan;
int duty_cycle = 50; // Default to 50% duty cycle until we read the ADC
int set_duty = 0;
int current_buffer = 1;
char key_char = '\0';
uint16_t num_entered = '\0';
uint16_t key_event = 0;
int key_event_count = 0;

volatile uint16_t *completed_raw_buffer;
absolute_time_t mode_switch_time_remaining_to_set;

int remaining_seconds = 0;
int remaining_days = 0;
int remaining_hours = 0;
int remaining_minutes = 0;
int remaining_seconds_seconds = 0;

volatile bool master_buffer_ready = false; // Flag to indicate a master buffer is full and ready for processing
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
enum SystemTimerState system_timer_state = MODE_DEFAULT;
enum EnterState
{
    NO_ENTRY,
    FIRST_PRESS,
    SECOND_PRESS
};
enum EnterState enter_state = NO_ENTRY;

void init_adc_combined_freerun();
void init_state_machine_led();
void init_timer_subsystem(); // Starts up the Alarm 0 infrastructure safely
void init_pwm_irq();
void keypad_init_pins();
void keypad_init_timer();
void start_new_blink_sequence();
void start_synchronized_adc_dma();
uint32_t enter_timed_run_first_state();
uint32_t enter_timed_run_second_state();
// ///////////////////////////////////////////////////////////////////

// Bring in your existing init function

int main()
{
    stdio_init_all();

    // Wait 3 seconds after boot so you can open your serial monitor in time
    sleep_ms(3000);
    absolute_time_t mode_switch_time_remaining_to_set;
    absolute_time_t last_key_press_time = get_absolute_time();
    // int duty_period[2];

    // 1. Hardware & Driver Initializations
    init_adc_combined_freerun();
    keypad_init_pins();
    keypad_init_timer();
    init_state_machine_led();
    init_timer_subsystem(); // Starts up the Alarm 0 infrastructure safely
    init_pwm_irq();

    // 3. Claim the master DMA channel from the system pool
    dma_master_chan = dma_claim_unused_channel(true);

    // 4. Synchronize and Kick Off Background Tasks
    // This starts the DMA channel first, then releases the ADC clock loop
    start_synchronized_adc_dma();
    for (;;)
    {
        key_event = key_pop();
        if (key_event != 0 ) // 50ms && absolute_time_diff_us(last_key_press_time, get_absolute_time()) > 50000
        {

            last_key_press_time = get_absolute_time();
            key_char = (char)(key_event & 0xFF);
        }
        else
        {
            key_event = 0; // Clear the key char if no new event to prevent accidental processing
            key_char = '\0';
        }

        // ... Keep your state logic here, but let's fix the ADC print below ...
        if (key_char == 'A' && system_timer_state == MODE_DEFAULT)
        {
            system_timer_state = MODE_MANUAL_ADJUST;
            mode_switch_time_remaining_to_set = get_absolute_time();
        }
        else if (key_char == '#' && system_timer_state == MODE_MANUAL_ADJUST && key_event != 0)
        {
            system_timer_state = ENTER_DAYS;
            set_duty = adc_fan_speed_control[0] * 100 / 4095;
            key_event = 0; // Clear the key event to prevent accidental processing in the new state
            // Explicitly force the hardware timer to update to the first press pattern
            // start_new_blink_sequence();
        }

        // Handle sequential inputs using clean state segregation
        if (key_char == '#' && key_event != 0 && (system_timer_state == ENTER_DAYS || system_timer_state == ENTER_HOURS || system_timer_state == ENTER_MINUTES))
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
            }
            key_event = 0; // Clear the key event to prevent accidental processing in the new state
            system_timer_state = NO_ENTRY;
        }
        switch (system_timer_state)
        {
        case MODE_MANUAL_ADJUST:
            break;
        case MODE_DEFAULT:
            break;
        case ENTER_DAYS:
            
            if (key_event != 0)
            {
                if (enter_state == NO_ENTRY)
                {
                    // printf("\n");
                    // printf("Entered no entry\n");
                    remaining_days = enter_timed_run_first_state();
                }
                else if (enter_state == FIRST_PRESS)
                {
                    remaining_days = (remaining_days * 10 + enter_timed_run_second_state());
                    remaining_seconds += 3600 * 24 * remaining_days;
                    // ONLY advance out of this state when the second press has completely finished processing

                    start_new_blink_sequence();
                }
                key_event = 0; // Clear the key event to prevent accidental processing in the new state
            }
            else
            {
                start_new_blink_sequence(); // Ensure we update the pattern immediately if a non-numeric key is pressed
            }
            // Ensure the timer pattern updates immediately on the first press
            break;
        case ENTER_HOURS:
            if (key_event != 0)
            {
                if (enter_state == NO_ENTRY)
                {
                    remaining_hours = enter_timed_run_first_state(system_timer_state, key_event);
                }
                else if (enter_state == FIRST_PRESS)
                {
                    remaining_hours = (remaining_hours * 10 + enter_timed_run_second_state(system_timer_state, key_event));
                    remaining_seconds += 3600 * remaining_hours;
                    // ONLY advance out of this state when the second press has completely finished processing
                    start_new_blink_sequence();
                }
                key_event = 0; // Clear the key event to prevent accidental processing in the new state
            }
            else
            {
                start_new_blink_sequence(); // Ensure we update the pattern immediately if a non-numeric key is pressed
            }
            // Ensure the timer pattern updates immediately on the first press
            break;
        case ENTER_MINUTES:
            if (key_event != 0)
            {
                if (enter_state == NO_ENTRY)
                {
                    remaining_minutes = enter_timed_run_first_state(system_timer_state, key_event);
                }
                else if (enter_state == FIRST_PRESS)
                {
                    remaining_minutes = (remaining_minutes * 10 + enter_timed_run_second_state(system_timer_state, key_event));
                    remaining_seconds += 60 * remaining_minutes;
                    // ONLY advance out of this state when the second press has completely finished processing
                    start_new_blink_sequence();
                }
                key_event = 0; // Clear the key event to prevent accidental processing in the new state
            }
            else
            {
                start_new_blink_sequence(); // Ensure we update the pattern immediately if a non-numeric key is pressed
            }
            // Ensure the timer pattern updates immediately on the first press
            break;
        case ENTER_SECONDS:
            if (key_event != 0)
            {
                if (enter_state == FIRST_PRESS)
                {
                    remaining_seconds_seconds = enter_timed_run_first_state(system_timer_state, key_event);
                }
                else if (enter_state == SECOND_PRESS)
                {
                    remaining_seconds_seconds = (remaining_seconds_seconds * 10 + enter_timed_run_second_state(system_timer_state, key_event));
                    remaining_seconds += remaining_seconds_seconds;
                    start_new_blink_sequence();
                }
                key_event = 0; // Clear the key event to prevent accidental processing in the new state
            }
            else
            {
                start_new_blink_sequence(); // Ensure we update the pattern immediately if a non-numeric key is pressed
            }
            // Ensure the timer pattern updates immediately on the first press
            break;
        }
        if (master_buffer_ready)
        {
            master_buffer_ready = false;

            // CRITICAL UPDATE: Extract from the safe atomic pointer snapshot
            uint16_t *raw_data = (uint16_t *)completed_raw_buffer;

            // Fallback protection just in case main loops before an IRQ finishes
            if (raw_data != NULL)
            {
                for (int i = 0; i < NUM_SAMPLES; i++)
                {
                    adc_duty_cycle_result[i] = raw_data[2 * i];     // Channel 2
                    adc_fan_speed_control[i] = raw_data[2 * i + 1]; // Channel 5
                }

                // Your math / duty logic remains here...

                if (system_timer_state == MODE_DEFAULT)
                {
                    duty_cycle = 50;
                }
                else if (system_timer_state == MODE_MANUAL_ADJUST)
                {
                    duty_cycle = adc_fan_speed_control[0] * 100 / 4095;
                    // measure_duty_cycle_period(adc_duty_cycle_result, duty_period);
                }
                else
                {
                    duty_cycle = set_duty;
                }
            }
        }

        

        

        fflush(stdout);
    }

    return 0;
}

// Replace your entire key-reading and state-switching blocks with this streamlined logic:
// printf("Entered Days\n");
//             switch (enter_state)
//             {
//             case NO_ENTRY:
//                 printf("Entry: NO_ENTRY\r");
//                 // Your no-entry logic block goes safely here without bleed-through...
//                 break;
//             case FIRST_PRESS:
//                 printf("Entry: FIRST_PRESS\r");
//                 // Your first press logic block goes safely here without bleed-through...
//                 break;

//             case SECOND_PRESS:
//                 printf("Entry: SECOND_PRESS\r");
//                 // Your second press logic block goes safely here without bleed-through...
//                 break;

//             default:
//                 break;
//             }
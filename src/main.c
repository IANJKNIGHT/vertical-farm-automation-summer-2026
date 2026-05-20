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
volatile uint16_t *completed_raw_buffer;
absolute_time_t mode_switch_time_remaining_to_set;

uint32_t remaining_seconds = 0;

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

void display_init_pins();
void display_init_timer();
void measure_duty_cycle_period(uint16_t *adc_result, int *duty_period);
void display_char_print(const char *buffer);
void start_synchronized_adc_dma();
void manual_mode_adjust_led_handler();
void init_set_time();
void init_timer_subsystem();
extern void init_adc_combined_freerun();
// void init_master_dma();
void init_pwm_irq();

extern void init_adc_read_pwm_freerun();
extern void keypad_init_pins();
extern void keypad_init_timer();
extern uint16_t key_pop();
extern void init_set_mode();
// ///////////////////////////////////////////////////////////////////

int enter_timed_run_first_state(enum SystemTimerState state, uint16_t key_event) 
{
    while (key_event != 0)
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
            printf("Invalid key press...\n");
            enter_state = FIRST_PRESS;
        }
    }
    return remaining_seconds;
}

int enter_timed_run_second_state(enum SystemTimerState state, uint16_t key_event) // fill in
{
    while (key_event != 0)
    {
        char key_char = (char)(key_event & 0xFF);
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
        printf("Invalid hours input. Please enter a value between 0 and 23.\n");
        remaining_seconds = 0;
        enter_state = FIRST_PRESS;
    }
    if ((state == ENTER_MINUTES || state == ENTER_SECONDS) && remaining_seconds > 59)
    {
        printf("Invalid minutes input. Please enter a value between 0 and 59.\n");
        remaining_seconds = 0;
        enter_state = FIRST_PRESS;
    }
    return remaining_seconds;
}



// Bring in your existing init function

int main()
{
    stdio_init_all();

    // Wait 3 seconds after boot so you can open your serial monitor in time
    sleep_ms(3000);
    absolute_time_t mode_switch_time_remaining_to_set;
    // int duty_period[2];
    
    // 1. Hardware & Driver Initializations
    init_adc_combined_freerun();
    init_state_machine_led();
    init_timer_subsystem(); // Starts up the Alarm 0 infrastructure safely
    init_pwm_irq();
    keypad_init_pins();
    keypad_init_timer();
    init_timer_subsystem(); // Configures Alarm 0 and registers handler
    
    // 3. Claim the master DMA channel from the system pool
    dma_master_chan = dma_claim_unused_channel(true);

    // 4. Synchronize and Kick Off Background Tasks
    // This starts the DMA channel first, then releases the ADC clock loop
    start_synchronized_adc_dma();
    for (;;)
    {
        uint16_t key_event = key_pop();
        if (key_event != 0)
        {
            key_char = (char)(key_event & 0xFF);
        }

        // ... Keep your state logic here, but let's fix the ADC print below ...
        if (key_char == 'A')
        {
            system_timer_state = MODE_MANUAL_ADJUST;
            mode_switch_time_remaining_to_set = get_absolute_time();
        }
        if (key_char == '#' && system_timer_state == MODE_MANUAL_ADJUST)
        {
            system_timer_state = ENTER_DAYS;
            enter_state = FIRST_PRESS;
            set_duty = adc_fan_speed_control[0] * 100 / 4095;
        }
        if (system_timer_state == ENTER_DAYS)
        {
            if (enter_state == FIRST_PRESS)
            {
                remaining_seconds = enter_timed_run_first_state(system_timer_state, key_event);
            }
            if (enter_state == SECOND_PRESS)
            {

                remaining_seconds = enter_timed_run_second_state(system_timer_state, key_event);
                remaining_seconds *= 24 * 3600; // Convert days to seconds
            }
            system_timer_state = ENTER_HOURS;
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
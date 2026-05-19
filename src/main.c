#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/irq.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "queue.h"

//////////////////////////////////////////////////////////////////////////////

#define NUM_SAMPLES 1000
#define MASTER_BUFFER_SIZE (NUM_SAMPLES * 2) // 2000 elements total
#define SPEED_SET_TIME 30                    // seconds

// Double-sized master ping-pong buffers
uint16_t master_buffer1[MASTER_BUFFER_SIZE];
uint16_t master_buffer2[MASTER_BUFFER_SIZE];

uint16_t adc_fan_speed_control[NUM_SAMPLES];
uint16_t adc_duty_cycle_result[NUM_SAMPLES];

int dma_master_chan;
int duty_cycle = 50; // Default to 50% duty cycle until we read the ADC
int set_duty = 0;
int current_buffer = 1;
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
extern void init_adc_combined_freerun();
// void init_master_dma();
void init_pwm_irq();

extern void init_adc_read_pwm_freerun();
extern void keypad_init_pins();
extern void keypad_init_timer();
extern uint16_t key_pop();
extern void init_set_mode();
// ///////////////////////////////////////////////////////////////////

// int enter_timed_run_first_state(enum SystemTimerState state, uint16_t key_event) // fill in
// {
//     while (key_event != 0)
//     {
//         char key_char = (char)(key_event & 0xFF);
//         switch (key_char)
//         {
//         case '0' ... '9':
//             remaining_seconds = 10 * (key_char - '0');
//             enter_state = SECOND_PRESS;
//             break;
//         default:
//             printf("Invalid key press for days input. Please enter digits 0-9.\n");
//             enter_state = FIRST_PRESS;
//         }
//     }
//     return remaining_seconds;
// }

// int enter_timed_run_second_state(enum SystemTimerState state, uint16_t key_event) // fill in
// {
//     while (key_event != 0)
//     {
//         char key_char = (char)(key_event & 0xFF);
//         switch (key_char)
//         {
//         case '0' ... '9':
//             remaining_seconds += (key_char - '0');
//             break;
//         case 'C':
//             // Clear the entry
//             remaining_seconds = 0;
//             enter_state = FIRST_PRESS;
//             break;
//         default:
//             printf("Invalid key press for days input. Please enter digits 0-9.\n");
//             enter_state = SECOND_PRESS;
//         }
//     }
//     if (state == ENTER_HOURS && remaining_seconds > 23)
//     {
//         printf("Invalid hours input. Please enter a value between 0 and 23.\n");
//         remaining_seconds = 0;
//         enter_state = FIRST_PRESS;
//     }
//     if ((state == ENTER_MINUTES || state == ENTER_SECONDS) && remaining_seconds > 59)
//     {
//         printf("Invalid minutes input. Please enter a value between 0 and 59.\n");
//         remaining_seconds = 0;
//         enter_state = FIRST_PRESS;
//     }
//     return remaining_seconds;
// }

// // int main()
// // {
// //     // Configures our microcontroller to
// //     // communicate over UART through the TX/RX pins
// //     stdio_init_all();

// //     // Step 3 - freerun
// //     dma_master_chan = dma_claim_unused_channel(true);

// //     absolute_time_t mode_switch_time_remaining_to_set;
// //     int duty_period[2];
// //     volatile char key_char = '\0';
// //     init_adc_combined_freerun();
// //     init_master_dma();
// //     init_pwm_irq();
// //     keypad_init_pins();
// //     keypad_init_timer();
// //     bool enter_manual_mode_message_sent = false;
// //     bool enter_days_input_message_sent = false;
// //     bool enter_hours_input_message_sent = false;
// //     bool enter_minutes_input_message_sent = false;
// //     bool enter_seconds_input_message_sent = false;
// //     // init_set_mode();

// //     for (;;)
// //     {
// //         // __wfi();
// //         // Wait until the single DMA handler flags that a master buffer is completely full

// //         uint16_t key_event = key_pop();
// //         if (key_event != 0)
// //         {
// //             key_char = (char)(key_event & 0xFF);
// //         }

//         if (key_char == 'A')
//         {
//             system_timer_state = MODE_MANUAL_ADJUST;
//             if (!enter_manual_mode_message_sent)
//             {
//                 // printf("Entered Manual Adjust Mode. Use the keypad to set the fan speed control value. Press # when done.\n");
//                 // printf("\n");
//                 enter_manual_mode_message_sent = true;
//             }
//             // printf("Entered Manual Adjust Mode. Use the keypad to set the fan speed control value. Press # when done.\n");
//             mode_switch_time_remaining_to_set = get_absolute_time();
//         }
// //         if ((system_timer_state == MODE_MANUAL_ADJUST || system_timer_state == ENTER_DAYS || system_timer_state == ENTER_HOURS || system_timer_state == ENTER_MINUTES || system_timer_state == ENTER_SECONDS) && system_timer_state != MODE_TIMED_RUN)
// //         {
// //             absolute_time_t now = get_absolute_time();
// //             if (absolute_time_diff_us(mode_switch_time_remaining_to_set, now) >= SPEED_SET_TIME * 1000000)
// //             {
// //                 system_timer_state = MODE_DEFAULT;
// //             }
// //         }
// //         if (key_char == '#' && system_timer_state == MODE_MANUAL_ADJUST)
// //         {
// //             system_timer_state = ENTER_DAYS;
// //         }
// //         if (system_timer_state == ENTER_DAYS)
// //         {
// //             if (!enter_days_input_message_sent)
// //             {
// //                 printf("Entered Days Input Mode. Use the keypad to enter the number of days for the timed run. Press * to move to hours input. Press C to clear entry.\n");
// //                 enter_days_input_message_sent = true;
// //             }
// //             printf("Entered Days Input Mode. Use the keypad to enter the number of days for the timed run. Press * to move to hours input. Press C to clear entry.\n");
// //             if (enter_state == FIRST_PRESS)
// //             {
// //                 printf("Waiting for first digit of days input...\n");
// //                 remaining_seconds = enter_timed_run_first_state(system_timer_state, key_event);
// //             }
// //             if (enter_state == SECOND_PRESS)
// //             {
// //                 printf("Waiting for second digit of days input or * to move to hours input...\n");
// //                 remaining_seconds = enter_timed_run_second_state(system_timer_state, key_event);
// //                 remaining_seconds *= 24 * 3600; // Convert days to seconds
// //             }
// //             system_timer_state = ENTER_HOURS;
// //         }
// //         if (key_char == '*' && system_timer_state == ENTER_HOURS)
// //         {
// //             if (!enter_hours_input_message_sent)
// //             {
// //                 printf("Entered Hours Input Mode. Use the keypad to enter the number of hours for the timed run. Press * to move to minutes input. Press C to clear entry.\n");
// //                 enter_hours_input_message_sent = true;
// //             }
// //             printf("Entered Hours Input Mode. Use the keypad to enter the number of hours for the timed run. Press * to move to minutes input. Press C to clear entry.\n");
// //             int remaining_seconds_for_hours = 0;
// //             if (enter_state == FIRST_PRESS)
// //             {
// //                 printf("Waiting for first digit of hours input...\n");
// //                 remaining_seconds_for_hours = enter_timed_run_first_state(system_timer_state, key_event);
// //             }
// //             if (enter_state == SECOND_PRESS)
// //             {
// //                 printf("Waiting for second digit of hours input or * to move to minutes input...\n");
// //                 remaining_seconds_for_hours = enter_timed_run_second_state(system_timer_state, key_event);
// //                 remaining_seconds_for_hours *= 3600; // Convert hours to seconds
// //                 remaining_seconds = remaining_seconds + remaining_seconds_for_hours;
// //             }
// //             system_timer_state = ENTER_MINUTES;
// //         }
// //         if (key_char == '*' && system_timer_state == ENTER_MINUTES)
// //         {
// //             if (!enter_minutes_input_message_sent)
// //             {
// //                 printf("Entered Minutes Input Mode. Use the keypad to enter the number of minutes for the timed run. Press * to move to seconds input. Press C to clear entry.\n");
// //                 enter_minutes_input_message_sent = true;
// //             }
// //             printf("Entered Minutes Input Mode. Use the keypad to enter the number of minutes for the timed run. Press * to move to seconds input. Press C to clear entry.\n");
// //             int remaining_seconds_for_minutes = 0;
// //             if (enter_state == FIRST_PRESS)
// //             {
// //                 printf("Waiting for first digit of minutes input...\n");
// //                 remaining_seconds_for_minutes = enter_timed_run_first_state(system_timer_state, key_event);
// //             }
// //             if (enter_state == SECOND_PRESS)
// //             {
// //                 printf("Waiting for second digit of minutes input or * to move to seconds input...\n");
// //                 remaining_seconds_for_minutes = enter_timed_run_second_state(system_timer_state, key_event);
// //                 remaining_seconds_for_minutes *= 60; // Convert minutes to seconds
// //                 remaining_seconds = remaining_seconds + remaining_seconds_for_minutes;
// //             }
// //             system_timer_state = ENTER_SECONDS;
// //         }
// //         if (key_char == '*' && system_timer_state == ENTER_SECONDS)
// //         {
// //             if (!enter_seconds_input_message_sent)
// //             {
// //                 printf("Entered Seconds Input Mode. Use the keypad to enter the number of seconds for the timed run. Press # when done. Press C to clear entry.\n");
// //                 enter_seconds_input_message_sent = true;
// //             }
// //             printf("Entered Seconds Input Mode. Use the keypad to enter the number of seconds for the timed run. Press # when done. Press C to clear entry.\n");
// //             int remaining_seconds_for_seconds = 0;
// //             if (enter_state == FIRST_PRESS)
// //             {
// //                 printf("Waiting for first digit of seconds input...\n");
// //                 remaining_seconds_for_seconds = enter_timed_run_first_state(system_timer_state, key_event);
// //             }
// //             if (enter_state == SECOND_PRESS)
// //             {
// //                 printf("Waiting for second digit of seconds input or # to finish input...\n");
// //                 remaining_seconds_for_seconds = enter_timed_run_second_state(system_timer_state, key_event);
// //                 remaining_seconds = remaining_seconds + remaining_seconds_for_seconds;
// //             }
// //             system_timer_state = MODE_TIMED_RUN;
// //             printf("Timed run set for %ld seconds. Starting timed run...\n", remaining_seconds);
// //         }
// //         if (master_buffer_ready)
// //         {
// //             master_buffer_ready = false; // Reset flag

// //             // Point to the buffer that just finished filling
// //             uint16_t *raw_data = (current_buffer == 2) ? master_buffer1 : master_buffer2;

// //             // 3. De-interleave the Round-Robin data
// //             // Since Ch 2 was selected first: Even indices = Ch 2, Odd indices = Ch 5
// //             for (int i = 0; i < NUM_SAMPLES; i++)
// //             {
// //                 adc_duty_cycle_result[i] = raw_data[2 * i];     // Channel 2 (GPIO 42)
// //                 adc_fan_speed_control[i] = raw_data[2 * i + 1]; // Channel 5 (GPIO 45)
// //             }
// //             if (system_timer_state == MODE_DEFAULT)
// //             {
// //                 duty_cycle = 50; // Default to 50% duty cycle until we read the ADC
// //             }
// //             if (system_timer_state == MODE_MANUAL_ADJUST || system_timer_state == MODE_TIMED_RUN)
// //             {
// //                 duty_cycle = adc_fan_speed_control[0] * 100 / 4095;
// //             }
// //             // 4. Process data safely now that it is separated
// //             // duty_cycle = adc_fan_speed_control[0] * 100 / 4095;
// //             measure_duty_cycle_period(adc_duty_cycle_result, duty_period);
// //             // printf("Output Duty Cycle: %d  \r", duty_cycle);
// //             printf("ADC only: %d \r", adc_fan_speed_control[0]);
// //             // printf("Input ADC (Ch5): %d, Duty Cycle: %d Output Duty Cycle: %d, Period: %d samples    \r", adc_fan_speed_control[0], duty_cycle, duty_period[0], duty_period[1]);
// //         }

// //         fflush(stdout);
// //     }
// //     return 0;
// // }

// int main()
// {
//     stdio_init_all();

//     // ... your other setup code ...
//     dma_master_chan = dma_claim_unused_channel(true);

//     absolute_time_t mode_switch_time_remaining_to_set;
//     int duty_period[2];
//     char key_char = '\0';
//     init_adc_combined_freerun();
//     init_master_dma();
//     init_pwm_irq();
//     keypad_init_pins();
//     keypad_init_timer();
//     bool enter_manual_mode_message_sent = false;
//     bool enter_days_input_message_sent = false;
//     bool enter_hours_input_message_sent = false;
//     bool enter_minutes_input_message_sent = false;
//     bool enter_seconds_input_message_sent = false;

//     // ADD THIS LINE: Track when we are allowed to print next
//     absolute_time_t next_print_time = get_absolute_time();

//     for (;;)
//     {
//         uint16_t key_event = key_pop();
//         if (key_event != 0) {
//             key_char = (char)(key_event & 0xFF);
//         }

//         // ... Keep your state logic here, but let's fix the ADC print below ...
//         if (master_buffer_ready)
//     {
//         master_buffer_ready = false;

//         // CRITICAL UPDATE: Extract from the safe atomic pointer snapshot
//         uint16_t *raw_data = (uint16_t *)completed_raw_buffer;

//         // Fallback protection just in case main loops before an IRQ finishes
//         if (raw_data != NULL)
//         {
//             for (int i = 0; i < NUM_SAMPLES; i++)
//             {
//                 adc_duty_cycle_result[i] = raw_data[2 * i];     // Channel 2
//                 adc_fan_speed_control[i] = raw_data[2 * i + 1]; // Channel 5
//             }

//             // Your math / duty logic remains here...
//             if (system_timer_state == MODE_DEFAULT) {
//                 duty_cycle = 50;
//             }
//             if (system_timer_state == MODE_MANUAL_ADJUST || system_timer_state == MODE_TIMED_RUN) {
//                 duty_cycle = adc_fan_speed_control[0] * 100 / 4095;
//                 measure_duty_cycle_period(adc_duty_cycle_result, duty_period);
//             }

//             if (absolute_time_diff_us(next_print_time, get_absolute_time()) >= 0)
//             {
//                 printf("ADC Raw: %4d | Duty: %d%%\n", adc_fan_speed_control[0], duty_cycle);
//                 next_print_time = delayed_by_ms(get_absolute_time(), 200);
//             }
//         }
//     }

//         fflush(stdout);
//     }
//     return 0;
// }

// Bring in your existing init function

int main()
{
    stdio_init_all();

    // Wait 3 seconds after boot so you can open your serial monitor in time
    sleep_ms(3000);
    absolute_time_t mode_switch_time_remaining_to_set;
    int duty_period[2];
    char key_char = '\0';
    init_adc_combined_freerun();
    // init_master_dma();
    init_pwm_irq();
    keypad_init_pins();
    keypad_init_timer();
    init_state_machine_led();
    bool enter_manual_mode_message_sent = false;
    bool enter_days_input_message_sent = false;
    bool enter_hours_input_message_sent = false;
    bool enter_minutes_input_message_sent = false;
    bool enter_seconds_input_message_sent = false;

    // Initialize your pins using your existing hardware setup function
    init_adc_combined_freerun();

    // CRITICAL: Force turn off Free-Run Mode and Round-Robin
    // We want to manually poll the registers ourselves

    int dma_master_chan = dma_claim_unused_channel(true);
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
            if (!enter_manual_mode_message_sent)
            {
                // printf("Entered Manual Adjust Mode. Use the keypad to set the fan speed control value. Press # when done.\n");
                // printf("\n");
                enter_manual_mode_message_sent = true;
            }
            // printf("Entered Manual Adjust Mode. Use the keypad to set the fan speed control value. Press # when done.\n");
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
            if (!enter_days_input_message_sent)
            {
                enter_days_input_message_sent = true;
            }
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

                // if (absolute_time_diff_us(next_print_time, get_absolute_time()) >= 0)
                // {
                //     printf("ADC Raw: %4d | Duty: %d%%\n", adc_fan_speed_control[0], duty_cycle);
                //     next_print_time = delayed_by_ms(get_absolute_time(), 200);
                // }
            }
        }

        fflush(stdout);
    }

    return 0;
}
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
char last_num_entered = '\0';
bool first_num_set = false;

volatile uint16_t *completed_raw_buffer;
absolute_time_t mode_switch_time_remaining_to_set;

int remaining_days = 0;
int remaining_hours = 0;
int remaining_minutes = 0;
int remaining_seconds_seconds = 0;
bool time_val_for_fan_displayed = false;
bool entry_too_large_error = false; // when the entered time is greater than 23 hours or 59 min/seconds

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
    SAVE_STATE_1,
    SECOND_PRESS,
    SAVE_STATE_2,
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
void noEntryLogic(char stable_key);
void firstPressLogic(char stable_key);
void save_state_1_logic(char stable_key);
void secondPressLogic(char stable_key);
void update_ui_leds_from_main(char stable_key);
void save_state_2_logic();
void debug_enter_state();
void debug_system_timer_state();
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
        if (key_event != 0 && (absolute_time_diff_us(last_key_press_time, get_absolute_time()) > 2000000)) // 50ms && absolute_time_diff_us(last_key_press_time, get_absolute_time()) > 50000
        {

            last_key_press_time = get_absolute_time();
            key_char = (char)(key_event & 0xFF);
            debug_system_timer_state();
            debug_enter_state();
        }
        else
        {
            key_event = 0;
        }
        char current_key_snapshot = key_char;

        switch (system_timer_state)
        {
        case MODE_DEFAULT:
            duty_cycle = 50;
            if (key_char == 'A')
            {
                system_timer_state = MODE_MANUAL_ADJUST;
                mode_switch_time_remaining_to_set = get_absolute_time();
            }
            break;
        case MODE_MANUAL_ADJUST:
            system_timer_state = (key_char == '#') ? ENTER_DAYS : (key_char == 'B') ? MODE_DEFAULT
                                                                                    : MODE_MANUAL_ADJUST;
            set_duty = adc_fan_speed_control[0] * 100 / 4095;
            break;
        default:
            break;
        }

        if (key_event != 0 && system_timer_state != MODE_DEFAULT && system_timer_state != MODE_MANUAL_ADJUST)
        {
            switch (enter_state)
            {
            case NO_ENTRY:
                noEntryLogic(current_key_snapshot);
                break;
            case FIRST_PRESS:
                firstPressLogic(current_key_snapshot);
                break;
            case SAVE_STATE_1:
                save_state_1_logic(current_key_snapshot);
                break;
            case SECOND_PRESS:
                secondPressLogic(current_key_snapshot);
                break;
            case SAVE_STATE_2:
                save_state_2_logic(current_key_snapshot);
                break;
            case ENTRY_TOO_LARGE:
                entry_too_large_logic();
            }
        }
        current_key_snapshot = first_num_set == true ? last_num_entered : current_key_snapshot;
        update_ui_leds_from_main(current_key_snapshot);

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

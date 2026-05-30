#include <stdio.h>
#include <stdlib.h>
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
bool linked_list_set = false;

volatile uint16_t *completed_raw_buffer;
absolute_time_t mode_switch_time_remaining_to_set;

int remaining_days = 0;
int remaining_hours = 0;
int remaining_minutes = 0;
int remaining_seconds_seconds = 0;
bool time_val_for_fan_displayed = false;
bool entry_too_large_error = false; // when the entered time is greater than 23 hours or 59 min/seconds
absolute_time_t mode_switch_time_remaining_to_set;
bool time_interval_head_set = false;

volatile bool master_buffer_ready = false; // Flag to indicate a master buffer is full and ready for processing
enum SystemTimerState
{
    MODE_DEFAULT,
    MODE_MANUAL_ADJUST,
    SEL_TIME_VALS,
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
    TOO_BIG
};
enum EnterState enter_state = NO_ENTRY;
struct timerInterval
{
    enum SystemTimerState timer_state;
    struct timerInterval *next;
    struct timerInterval *prev;
};

struct timerInterval *timer_interval_head = NULL;

enum servoPosition
{
    HOLD,
    SWEEP_UP,
    SWEEP_DOWN
};

enum servoPosition current_servo_position = HOLD;
bool head_memory_allocated_4_timer = false;
enum SystemTimerState firstTimerState;
int timer_interval_index = 0;
int virtual_servo_angle = 0;

struct nv_recovery_t
{
    uint32_t magic_number; // Signature validation tag (0xDEADBEEF)
    uint32_t set_duty;     // Fan duty cycle
    int32_t remaining_days;
    int32_t remaining_hours;
    int32_t remaining_minutes;
    int32_t remaining_seconds;
    uint8_t padding[232]; // Pads struct to exactly 256 bytes (1 Page)
} nv_recovery_t;
void measure_duty_cycle_period(uint16_t *adc_result, int *duty_period);
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
void setTimeIntervals();
void destroyList();
void printConfiguredIntervals();
void init_read_rpm_pwm();
void run_continuous_servo_sweep();
uint16_t get_raw_pwm_counter_value();
extern struct timerInterval *findTimeEntryType();
;
uint32_t enter_timed_run_first_state();
uint32_t enter_timed_run_second_state();
// ///////////////////////////////////////////////////////////////////

// Bring in your existing init function

int main()
{
    stdio_init_all();

    // Define global states variables locally for context representation
    // int remaining_days, remaining_hours, remaining_minutes, remaining_seconds_seconds;
    // uint32_t current_duty_cycle;

    // nv_recovery_t saved_state;

    // if (load_state_from_flash(&saved_state)) {
    //     printf("Power loss recovery detected! Restoring last operation parameters...\n");
    //     current_duty_cycle        = saved_state.set_duty;
    //     remaining_days            = saved_state.remaining_days;
    //     remaining_hours           = saved_state.remaining_hours;
    //     remaining_minutes          = saved_state.remaining_minutes;
    //     remaining_seconds_seconds = saved_state.remaining_seconds;
    // } else {
    //     printf("Cold boot identified. Booting up with generic runtime options...\n");
    //     current_duty_cycle        = 0;
    //     remaining_days            = 0;
    //     remaining_hours           = 0;
    //     remaining_minutes          = 0;
    //     remaining_seconds_seconds = 0;
    // }

    // Wait 3 seconds after boot so you can open your serial monitor in time
    sleep_ms(3000);

    absolute_time_t last_key_press_time = get_absolute_time();
    int duty_period[2];

    // 1. Hardware & Driver Initializations
    init_adc_combined_freerun();
    keypad_init_pins();
    keypad_init_timer();
    init_state_machine_led();
    init_timer_subsystem(); // Starts up the Alarm 0 infrastructure safely
    init_pwm_irq();
    init_read_rpm_pwm();

    // 3. Claim the master DMA channel from the system pool
    dma_master_chan = dma_claim_unused_channel(true);
    absolute_time_t next_rpm_sample_time;
    uint16_t last_edge_count = 0;
    int cached_rpm = 0;
    next_rpm_sample_time = make_timeout_time_ms(500);

    // 4. Synchronize and Kick Off Background Tasks
    // This starts the DMA channel first, then releases the ADC clock loop
    start_synchronized_adc_dma();
    for (;;)
    {
        // run_continuous_servo_sweep();
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
            duty_cycle = 20;
            if (key_char == 'A')
            {
                system_timer_state = MODE_MANUAL_ADJUST;
                mode_switch_time_remaining_to_set = get_absolute_time();
            }
            break;
        case MODE_MANUAL_ADJUST:
            set_duty = adc_fan_speed_control[0] * 100 / 4095;
            switch (key_char)
            {
            case '#':
                system_timer_state = SEL_TIME_VALS;
                break;
            case 'B':
                system_timer_state = MODE_DEFAULT;
                break;
            }
            system_timer_state = (key_char == '#') ? SEL_TIME_VALS : (key_char == 'B') ? MODE_DEFAULT
                                                                                       : MODE_MANUAL_ADJUST;
            break;
        case SEL_TIME_VALS:
            if (!linked_list_set && key_event != 0)
            {
                setTimeIntervals();
            }

            if (linked_list_set && timer_interval_head != NULL)
            {
                system_timer_state = timer_interval_head->timer_state;
            }

            // else if (linked_list_set == true && key_char == 'B')
            // {
            //     system_timer_state = MODE_DEFAULT;
            //     destroyList();
            // }

            break;
        case MODE_TIMED_RUN:
            if (absolute_time_diff_us(mode_switch_time_remaining_to_set, get_absolute_time()) >= 1000000)
            {
                // 1. Reset the reference timestamp so this block triggers exactly 1 second from now
                mode_switch_time_remaining_to_set = get_absolute_time();

                // 2. Collapse everything into a single, comprehensive pool of total remaining seconds
                uint32_t total_remaining_seconds = remaining_seconds_seconds +
                                                   (remaining_minutes * 60) +
                                                   (remaining_hours * 3600) +
                                                   (remaining_days * 86400);

                // 3. Decrement the clock by exactly 1 second if time hasn't completely run out
                if (total_remaining_seconds > 0)
                {
                    total_remaining_seconds--;

                    // 4. Extract the newly updated time divisions back out of our unified seconds pool
                    remaining_days = total_remaining_seconds / 86400;
                    uint32_t days_remainder = total_remaining_seconds % 86400;

                    remaining_hours = days_remainder / 3600;
                    uint32_t hours_remainder = days_remainder % 3600;

                    remaining_minutes = hours_remainder / 60;
                    remaining_seconds_seconds = hours_remainder % 60;
                }
                else
                {
                    system_timer_state = MODE_DEFAULT;
                }

                // Optional diagnostics printout to check your work in the serial console
                printf("Countdown -> D:%d H:%02d M:%02d S:%02d\n",
                       remaining_days, remaining_hours, remaining_minutes, remaining_seconds_seconds);
                mode_switch_time_remaining_to_set = get_absolute_time();
            }
            break;
        default:
            break;
        }

        if (key_event != 0 && system_timer_state != MODE_DEFAULT && system_timer_state != MODE_MANUAL_ADJUST && entry_too_large_error == false)
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
            default:
                // printf("TOO BIG\n");
                break;
            }
        }
        if (enter_state == TOO_BIG)
        {
            start_new_blink_sequence();
        }
        current_key_snapshot = first_num_set == true ? last_num_entered : current_key_snapshot;
        update_ui_leds_from_main(current_key_snapshot);

        if (time_reached(next_rpm_sample_time))
        {
            next_rpm_sample_time = make_timeout_time_ms(500); // Sample every half-second

            uint16_t current_edge_count = get_raw_pwm_counter_value();

            // Explicitly handle 16-bit register rollover protection math safely
            uint16_t delta_edges = current_edge_count - last_edge_count;
            last_edge_count = current_edge_count;

            // Math: We sampled for 500ms (1/2 of a second).
            // delta_edges * 2 = Edges per second.
            // (Edges per second / 2 pulses per rev) * 60 seconds = RPM
            // Simplified Math: delta_edges * 60
            cached_rpm = delta_edges * 60;
        }

        if (master_buffer_ready)
        {
            master_buffer_ready = false;
            uint16_t *raw_data = (uint16_t *)completed_raw_buffer;

            if (raw_data != NULL)
            {
                for (int i = 0; i < NUM_SAMPLES; i++)
                {
                    adc_duty_cycle_result[i] = raw_data[2 * i];
                    adc_fan_speed_control[i] = raw_data[2 * i + 1];
                }

                if (system_timer_state == MODE_MANUAL_ADJUST)
                {
                    duty_cycle = adc_fan_speed_control[0] * 100 / 4095;
                    measure_duty_cycle_period(adc_duty_cycle_result, duty_period);

                    // Print the cached value instantly with ZERO cycle delays!
                    printf("Duty Cycle: %d | Measured RPM: %d\r", duty_cycle, cached_rpm);
                }
                // ... rest of state assignment paths ...
            }
        }
        fflush(stdout);
    }
    return 0;
}

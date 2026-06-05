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
#include "hardware/pwm.h"

//////////////////////////////////////////////////////////////////////////////

#define NUM_SAMPLES 1000
#define MASTER_BUFFER_SIZE (NUM_SAMPLES * 2) // 2000 elements total
#define SPEED_SET_TIME 30                    // seconds
#define PWM_SAMPLING_4_Vfg 20                // ms
#define SERVO_PERIOD_MS 20                      // Standard servo PWM period is 20ms (50Hz)
#define MAX_FAN_RPM 15600
#define TELEMETRY_PRINT_INTERVAL_MS 200 // Change this to 500 or 1000 if you want it slower!
#define HOLD_STILL_TIME_MS 2000
#define MOVE_4_5_DEGS_MS 100 // ms

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
int servo_middle_position_pulse_ms_times_100 = 150; // 1.5ms pulse width for 90 degrees
int servo_180_position_pulse_ms_times_100 = 200;    // 2ms pulse width for 180 degrees
int servo_0_position_pulse_ms_times_100 = 100;      // 1ms pulse width for 0 degrees

volatile uint16_t *completed_raw_buffer;
absolute_time_t mode_switch_time_remaining_to_set;

int fan_spd_control_gpio = 25;
int servo_pwm_gpio_pin = 29; 
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
    MODE_PID_TUNING,
    SWEEP_SERVO,
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
    HOLD_90,
    ROTATE_180,
    ROTATE_0
};
enum servoPosition current_servo_position = HOLD_90;
enum servoPosition servo_transition_state = ROTATE_0;

typedef struct
{
    float Kp;
    float Ki;
    float Kd;
    float prev_error;
    float integral_error;
    float output;       /* output of the PID */
    float sam_rate;     /* sampling rate */
    float integral_max; /* Maximum of the error integral */
    float pid_max;      /* Maximum of the PID */
} pid_instance;

enum pid_typedef
{
    pid_ok,
    pid_numerical
};

enum pid_typedef apply_pid(pid_instance *pid, float input_error);

bool head_memory_allocated_4_timer = false;
enum SystemTimerState firstTimerState;
int timer_interval_index = 0;
int virtual_servo_angle = 0;

void measure_duty_cycle_period(uint16_t *adc_result, int *duty_period);
void init_adc_combined_freerun();
void init_state_machine_led();
void init_timer_subsystem(); // Starts up the Alarm 0 infrastructure safely
// void init_pwm_irq();
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
void init_all_system_pwms();
void debug_enter_state();
void debug_system_timer_state();
void print_pid_dashboard(int target, int current, pid_instance *pid);
void setTimeIntervals();
// void pwm_control_servo();
void destroyList();
void printConfiguredIntervals();
// void init_servo_position();
void init_read_rpm_pwm();
// void init_pwm_measure_pin(uint gpio_input);
void setup_clean_test_pwm();
uint32_t get_pulse_width_ticks(uint gpio_input);
void run_continuous_servo_sweep();
// void init_servo_pwm_irq();
void set_servo_pwm_speed();
int servo_pulse_width_ms_times_100 = 150; // Global variable to hold the current servo duty cycle, accessible across files
int target_rpm = 1000;        // Example target RPM for the fan
uint16_t get_raw_pwm_counter_value();
extern struct timerInterval *findTimeEntryType();
struct nv_recovery_t
{
    uint32_t magic_number; // Signature validation tag (0xDEADBEEF)
    uint32_t duty_cycle;   // Fan duty cycle
    int32_t remaining_days;
    int32_t remaining_hours;
    int32_t remaining_minutes;
    int32_t remaining_seconds;
    uint8_t padding[232]; // Pads struct to exactly 256 bytes (1 Page)
};

bool load_state_from_flash(struct nv_recovery_t *state_out);
uint32_t enter_timed_run_first_state();
uint32_t enter_timed_run_second_state();
// ///////////////////////////////////////////////////////////////////

// Bring in your existing init function

int main()
{
    stdio_init_all();

    // Define global states variables locally for context representation
    // int remaining_days, remaining_hours, remaining_minutes, remaining_seconds_seconds;
    uint32_t current_duty_cycle;
    struct nv_recovery_t saved_state;

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
    init_read_rpm_pwm();
    init_all_system_pwms();

    // 3. Claim the master DMA channel from the system pool
    dma_master_chan = dma_claim_unused_channel(true);

    absolute_time_t next_rpm_sample_time;
    absolute_time_t timer_hold_plus_90 = make_timeout_time_ms(HOLD_STILL_TIME_MS); // Start the first timer for the initial HOLD_90 position
    absolute_time_t timer_rotate_180;
    bool rotating_up = true;
    absolute_time_t timer_hold_minus_90;
    absolute_time_t timer_rotate_0;
    uint16_t last_edge_count = 0;
    int servo_pwm_counter = 0;

    int cached_rpm = 0;
    next_rpm_sample_time = make_timeout_time_ms(PWM_SAMPLING_4_Vfg);
    absolute_time_t next_telemetry_print_time = make_timeout_time_ms(TELEMETRY_PRINT_INTERVAL_MS);

    static pid_instance mota_pid = {.Kd = 0.000,
                                    .integral_error = 0,
                                    .Ki = 0.05,
                                    .integral_max = 200,
                                    .prev_error = 0,
                                    .output = 0,
                                    .Kp = .05,
                                    .pid_max = 100,
                                    .sam_rate = PWM_SAMPLING_4_Vfg / 1000.0f};

    // 4. Synchronize and Kick Off Background Tasks
    absolute_time_t next_flash_save_time = make_timeout_time_ms(30000);
    // This starts the DMA channel first, then releases the ADC clock loop
    start_synchronized_adc_dma();
    setup_clean_test_pwm();
    for (;;)
    {
        // run_continuous_servo_sweep();
        // if (time_reached(next_flash_save_time))
        // {
        // // Reset the timer for the next 30 seconds
        //     next_flash_save_time = make_timeout_time_ms(30000);

        //     printf("Auto-saving state to flash...\n");

        //     // Pass your current live variables into the flash save function
        //     save_state_to_flash(
        //         duty_cycle,
        //         remaining_days,
        //         remaining_hours,
        //         remaining_minutes,
        //         remaining_seconds_seconds
        //     );
        // }
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

        if (time_reached(next_rpm_sample_time))
        {
            next_rpm_sample_time = make_timeout_time_ms(PWM_SAMPLING_4_Vfg); // Sample every half-second

            uint16_t current_edge_count = get_raw_pwm_counter_value();

            // Explicitly handle 16-bit register rollover protection math safely
            uint16_t delta_edges = current_edge_count - last_edge_count;
            last_edge_count = current_edge_count;

            // Math: We sampled for 20ms (1/50 of a second).
            // delta_edges * 10 = Edges per second.
            // (Edges per second / 2 pulses per rev) * 60 seconds = RPM
            // Simplified Math: delta_edges * 300
            cached_rpm = delta_edges * (1000 / PWM_SAMPLING_4_Vfg) * (60 / 2);
            // Inside your buffer processing logic:
            // Format: Time(ms), TargetRPM, CurrentRPM, PID_Output
            if (time_reached(next_telemetry_print_time))
            {
                next_telemetry_print_time = make_timeout_time_ms(TELEMETRY_PRINT_INTERVAL_MS);
                // printf("Num edges: %d | Sampling Rate: 100ms\n", delta_edges);
                // This now prints elegantly at your throttled rate (e.g., 200ms)
                // print_pid_dashboard(target_rpm, cached_rpm, &mota_pid);
            }
        }
        if (time_reached(next_telemetry_print_time))
        {
            next_telemetry_print_time = make_timeout_time_ms(TELEMETRY_PRINT_INTERVAL_MS);

            // This now prints elegantly at your throttled rate (e.g., 200ms)
            // print_pid_dashboard(target_rpm, cached_rpm, &mota_pid);
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
                    // measure_duty_cycle_period(adc_duty_cycle_result, duty_period);

                    // Print the cached value instantly with ZERO cycle delays!
                    // printf("Duty Cycle: %d | Measured RPM: %d\n", duty_cycle, cached_rpm);
                }
                // ... rest of state assignment paths ...
            }
        }
        // set_servo_pwm_speed();
        // measure_duty_cycle_period(adc_duty_cycle_result, duty_period);
        // printf("Servo Duty Cycle: %d\n", duty_period[0]);
        
        // switch (current_servo_position)
        // {
        //     case HOLD_90:
        //     {
        //         if (servo_transition_state == ROTATE_0 && time_reached(timer_hold_plus_90))
        //         {
        //             current_servo_position = ROTATE_180;
        //             timer_rotate_180 = make_timeout_time_ms(MOVE_4_5_DEGS_MS);
        //             servo_transition_state = HOLD_90;
        //         }
        //         else if (servo_transition_state == ROTATE_180 && time_reached(timer_hold_minus_90))
        //         {
        //             current_servo_position = ROTATE_0;
        //             timer_rotate_0 = make_timeout_time_ms(MOVE_4_5_DEGS_MS);
        //         }
        //         break;
        //     }
        //     case ROTATE_180:
        //     {
        //         if (servo_transition_state == HOLD_90 && time_reached(timer_rotate_180))
        //         {
        //             servo_pulse_width_ms_times_100 = rotating_up ? servo_pulse_width_ms_times_100 + 5 : servo_pulse_width_ms_times_100 - 5;
        //             if (servo_pulse_width_ms_times_100 >= servo_180_position_pulse_ms_times_100)
        //             {
        //                 servo_pulse_width_ms_times_100 = servo_180_position_pulse_ms_times_100;
        //                 rotating_up = false;
        //             }
        //             timer_rotate_180 = make_timeout_time_ms(MOVE_4_5_DEGS_MS);
        //             current_servo_position = servo_pulse_width_ms_times_100 == servo_middle_position_pulse_ms_times_100 ? HOLD_90 : ROTATE_180;
        //             servo_transition_state = ROTATE_180;
        //         }
        //         break;
        //     }
        //     case ROTATE_0:
        //     {
        //         if (servo_transition_state == HOLD_90 && time_reached(timer_rotate_0))
        //         {
        //             servo_pulse_width_ms_times_100 = rotating_up ? servo_pulse_width_ms_times_100 + 5 : servo_pulse_width_ms_times_100 - 5;
        //             if (servo_pulse_width_ms_times_100 <= servo_0_position_pulse_ms_times_100)
        //             {
        //                 servo_pulse_width_ms_times_100 = servo_0_position_pulse_ms_times_100;
        //                 rotating_up = true;
        //             }
        //             timer_rotate_0 = make_timeout_time_ms(MOVE_4_5_DEGS_MS);
        //             current_servo_position = servo_pulse_width_ms_times_100 == servo_middle_position_pulse_ms_times_100 ? HOLD_90 : ROTATE_0;
        //             servo_transition_state = ROTATE_0;
        //         }
        //         break;
        //     }
        // }
        // pwm_set_gpio_level(servo_pwm_gpio_pin, (float) servo_pulse_width_ms_times_100 / (SERVO_PERIOD_MS * 100)); // Scale 100-200us pulse width to 625-1250 level for 10kHz PWM with 125MHz clock and wrap of 125000
        
        pwm_set_gpio_level(servo_pwm_gpio_pin, 10); // Scale 100-200us pulse width to 625-1250 level for 10kHz PWM with 125MHz clock and wrap of 125000
        // measure_duty_cycle_period(adc_duty_cycle_result, duty_period);

        switch (system_timer_state)
        {
        case MODE_DEFAULT:
        {
            if (target_rpm < 7000)
            {
                // Ultra-stable parameters for your primary low-speed operations
                mota_pid.Kp = 0.04f;
                mota_pid.Ki = 0.05f;
                mota_pid.integral_max = 150;
            }
            else
            {
                // Aggressive parameters to push past the high-speed aerodynamic wall
                mota_pid.Kp = 0.08f;
                mota_pid.Ki = 0.20f;
                mota_pid.integral_max = 250;
            }
            float error_percent = (float)(target_rpm - cached_rpm);
            apply_pid(&mota_pid, error_percent);

            int final_pwm = (int)mota_pid.output;

            // Clamp it between safe hardware limits
            if (final_pwm > 100)
                final_pwm = 100;
            if (final_pwm < 10)
                final_pwm = 10;
            // if (target_rpm >= .18 * MAX_FAN_RPM && final_pwm < 0)
            //     final_pwm = 0;

            duty_cycle = final_pwm;
            pwm_set_gpio_level(fan_spd_control_gpio, duty_cycle); // Scale duty cycle to ADC range for testing
            // duty_cycle = 50; // Comment this out to enable PID control and test your tuning changes in real-time!
            // measure_duty_cycle_period(adc_duty_cycle_result, duty_period);
            // printf("Duty Cycle: %d | Measured RPM: %d\n", duty_period[0], cached_rpm);

            if (key_char == 'A')
            {
                system_timer_state = MODE_MANUAL_ADJUST;
                mode_switch_time_remaining_to_set = get_absolute_time();
                key_char = '\0'; // Clear it so it doesn't instantly re-trigger
            }
            else if (key_char == 'C')
            {
                system_timer_state = MODE_PID_TUNING;
                key_char = '\0';
            }
            break;
        }
        case MODE_MANUAL_ADJUST:
        {
            // Sync both variables so your hardware and diagnostic metrics align
            duty_cycle = adc_fan_speed_control[0] * 100 / 4095;
            pwm_set_gpio_level(fan_spd_control_gpio, duty_cycle); 

            if (key_char == '#')
            {
                system_timer_state = SEL_TIME_VALS;
                key_char = '\0';
            }
            else if (key_char == 'B')
            {
                system_timer_state = MODE_DEFAULT;
                key_char = '\0';
            }
            break; // Clean exit. Ternary trap removed!
        }
        case MODE_PID_TUNING: // Click 'C' to go from Default -> PID Tuning mode, then use keys 1-6 to adjust Kp/Ki and target RPM in real-time while the PID is actively controlling the fan. Click '#' when you're happy with your tuning to save settings and return to normal operation
            // Let the grower/engineer pick WHICH variable to tune using letters
            if (key_char == '1')
            {
                mota_pid.Kp += 0.01f;
            } // Increment Kp }          }
            else if (key_char == '2')
            {
                mota_pid.Kp -= 0.01f;
            } // Decrement Kp
            else if (key_char == '3')
            {
                mota_pid.Ki += 0.005f;
            } // Fine-tune Ki
            else if (key_char == '4')
            {
                mota_pid.Ki -= 0.005f;
            } // Fine-tune Ki
            else if (key_char == '5')
            {
                target_rpm += 4000;
            } // Test a higher fan speed step response
            else if (key_char == '6')
            {
                target_rpm -= 4000;
            } // Test a lower fan speed step response
            else if (key_char == '7')
            {
                mota_pid.Kp += .1f;
            }
            else if (key_char == '8')
            {
                mota_pid.Kp -= .1f;
            }
            else if (key_char == '9')
            {
                mota_pid.Kd -= 0.001f;
            }
            else if (key_char == '#')
            {
                system_timer_state = MODE_DEFAULT;
            }
            // Print a live tuning dashboard to your Python script or Serial terminal
            // printf("TUNING: Kp=%0.3f | Ki=%0.3f | Target=%d \r",
            //        mota_pid.Kp, mota_pid.Ki, target_rpm);

            key_char = '\0'; // Clear input flag
            break;
        // case MODE_MANUAL_ADJUST:
        //     set_duty = adc_fan_speed_control[0] * 100 / 4095;
        //     switch (key_char)
        //     {
        //     case '#':
        //         system_timer_state = SEL_TIME_VALS;
        //         break;
        //     case 'B':
        //         system_timer_state = MODE_DEFAULT;
        //         break;
        //     }
        //     // system_timer_state = (key_char == '#') ? SEL_TIME_VALS : (key_char == 'B') ? MODE_DEFAULT
        //                                                                                : MODE_MANUAL_ADJUST;
        //     break;
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

        fflush(stdout);
    }
    return 0;
}

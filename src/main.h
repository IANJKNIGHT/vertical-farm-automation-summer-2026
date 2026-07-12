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
#include "hardware/i2c.h"
#include "tftspi.h"
#include "menu.h"
#include "read_adc.h"
#include "num_push_fsm.h"
#include "fan_feedback.h"

//////////////////////////////////////////////////////////////////////////////

#define NUM_SAMPLES 1000
#define MASTER_BUFFER_SIZE (NUM_SAMPLES * 2) // 2000 elements total
#define SPEED_SET_TIME 30                    // seconds
#define PWM_SAMPLING_4_Vfg 20                // ms
#define SERVO_PERIOD_MS 20                   // Standard servo PWM period is 20ms (50Hz)
#define MAX_FAN_RPM 15600
#define TELEMETRY_PRINT_INTERVAL_MS 200 // Change this to 500 or 1000 if you want it slower!
#define HOLD_STILL_TIME_MS 2000
#define MOVE_4_5_DEGS_MS 100 // ms
#define I2C_PORT i2c0
#define I2C_SDA_PIN 26
#define I2C_SCL_PIN 25

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
enum SystemTimerState system_timer_state = MODE_DEFAULT;

enum EnterState enter_state = NO_ENTRY;

struct timerInterval *timer_interval_head = NULL;

enum servoPosition
{
    HOLD_90,
    ROTATE_180,
    ROTATE_0
};
enum servoPosition current_servo_position = HOLD_90;
enum servoPosition servo_transition_state = ROTATE_0;

enum pid_typedef
{
    pid_ok,
    pid_numerical
};

enum pid_typedef apply_pid(pid_instance *pid, float input_error);

// Logic for the TFT display
enum ButtonInput current_button_press = BTN_NONE;

bool head_memory_allocated_4_timer = false;
enum SystemTimerState firstTimerState;
int timer_interval_index = 0;
int virtual_servo_angle = 0;


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
int target_rpm = 1000;                    // Example target RPM for the fan
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
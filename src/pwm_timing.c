#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/irq.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"

#define NUM_SAMPLES 1000
#define ALARM_NUM0 0
#define ALARM1_NUM 1
#define ALARM0_IRQ timer_hardware_alarm_get_irq_num(timer_hw, ALARM_NUM0)
#define ALARM1_IRQ timer_hardware_alarm_get_irq_num(timer_hw, ALARM1_NUM)

extern char key_char;
static int num_edges = 0;
static uint64_t rpm_1_rising_edge;
static uint64_t rpm_4_rising_edge;
extern uint64_t measured_rpm;
int buttonA_col_gpio = 4;
int vfg_gpio = 31;
static int vfg_pwm_freq_alarm_num = -1;
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

void set_default()
{
    // Clear the interrupt flag
    timer_hw->intr = 1u << ALARM_NUM0;

    // Reset the system timer state to default
    system_timer_state = MODE_DEFAULT;
}

void return_to_default_after_timeout(uint32_t timeout_ms)
{
    // Set the alarm to trigger after the specified timeout
    timer_hw->alarm[ALARM_NUM0] = timer_hw->timerawl + timeout_ms * 1000; // Convert ms to us
}

void set_mode_handler()
{
    // Clear the interrupt flag
    if (key_char == 'A')
    {
        system_timer_state = MODE_MANUAL_ADJUST;
    }
}

void init_set_mode()
{
    // Enable the interrupt for our alarm (the timer outputs 4 alarm irqs)
    // 1. Initialize the pin hardware routing block
    gpio_init(buttonA_col_gpio);
    gpio_set_dir(buttonA_col_gpio, GPIO_IN);
    gpio_pull_down(buttonA_col_gpio);
    // Set irq handler for alarm irq
    gpio_set_irq_enabled_with_callback(buttonA_col_gpio, GPIO_IRQ_EDGE_RISE, true, &set_mode_handler);
    // gpio_set_irq_enabled(buttonA_col_gpio, GPIO_IRQ_EDGE_RISE, true);
    // gpio_add_raw_irq_handler(buttonA_col_gpio, set_mode_handler);
    // irq_set_enabled(IO_IRQ_BANK0, true);
}

// 
static uint pwm_slice_num;

void init_read_rpm_pwm() {
    // 1. Tell the RP2350 that this pin is controlled by the PWM peripheral block, not the GPIO block
    gpio_set_function(vfg_gpio, GPIO_FUNC_PWM);
    gpio_pull_up(vfg_gpio);
    
    // Find out which hardware PWM slice is tied to your physical Pin 30
    pwm_slice_num = pwm_gpio_to_slice_num(vfg_gpio);

    // 2. Configure the slice to run as a counter measuring RISING edges
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv_mode(&config, PWM_DIV_B_RISING); // Count rising edges on the B pin
    

    pwm_config_set_wrap(&config, 0xFFFF); // Max out the wrap so we can count as many edges as possible before it rolls over
    // Initialize the hardware block with our configuration parameters
    pwm_init(pwm_slice_num, &config, true);
    // pwm_set_enabled(pwm_slice_num, true);
}

uint16_t get_raw_pwm_counter_value() {
    return pwm_get_counter(pwm_slice_num);
}
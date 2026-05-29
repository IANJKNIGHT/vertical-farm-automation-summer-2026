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
int buttonA_col_gpio = 4;
int vfg_gpio = 30;
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


void init_count_calculate_rpm()
{
    // Set the alarm to trigger every 1 second (1000 ms)
    vfg_pwm_freq_alarm_num = hardware_alarm_claim_unused(true);

    uint irq_num = TIMER0_IRQ_0 + vfg_pwm_freq_alarm_num;

    hw_set_bits(&timer_hw->inte, 1u << vfg_pwm_freq_alarm_num);

    irq_set_exclusive_handler(irq_num, );
}

void read_pwm_handler()
{
    // Clear the interrupt flag for the GPIO pin
    uint32_t mask = 1u << (vfg_gpio & 0x1fu);
    gpio_acknowledge_irq(vfg_gpio, mask);

    // Read the current state of the GPIO pin to determine if it's a rising edge
    if (gpio_get(vfg_gpio))
    {
        // Handle the rising edge event (e.g., read PWM value, update state, etc.)
        // For demonstration, we'll just print a message
        
    }
}

void init_read_pwm()
{
    uint32_t mask = 1u << (vfg_gpio & 0x1fu);
    gpio_init(vfg_gpio);
    gpio_set_dir(vfg_gpio, GPIO_IN);
    gpio_pull_down(vfg_gpio);

    gpio_set_exclusive_irq_handler(vfg_gpio, &read_pwm_handler);
    gpio_acknowledge_irq(vfg_gpio, mask);
    gpio_set_irq_enabled(vfg_gpio, GPIO_IRQ_EDGE_RISE, true);
    irq_set_enabled(IO_IRQ_BANK0, true);
}
// int read_fan_rpm()
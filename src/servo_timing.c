#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/irq.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"

absolute_time_t last_servo_tick;
int servo_runtime_seconds = 0;
extern int servo_duty_cycle;
bool moving_up = true;
extern absolute_time_t last_key_press_time;
int gpio_servo_control = 29; // GPIO pin connected to the servo control line

void setup_clean_test_pwm() {
    uint gpio_out = 33;
    
    // 1. Tell the RP2040 that Pin 29 is controlled by the PWM hardware
    gpio_set_function(gpio_out, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(gpio_out);
    uint channel = pwm_gpio_to_channel(gpio_out);

    // 2. Get a default configuration struct
    pwm_config config = pwm_get_default_config();

    // 3. Set Frequency to 1 kHz:
    // System Clock (125 MHz) / Clock Divisor (125) = 1 MHz tick rate.
    // With a 1 MHz tick rate, wrapping at 999 ticks gives a 1,000 tick period (1 kHz frequency).
    // pwm_config_set_clkdiv(&config, 125.0f); 
    // pwm_config_set_wrap(&config, 999);

    // 4. Load the configuration and enable the PWM slice
    pwm_init(slice_num, &config, true);

    // 5. Set Duty Cycle to 50%:
    // 50% of our 1000-tick period (0 to 999) is exactly 500 ticks.
    pwm_set_chan_level(slice_num, channel, 500);
}
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
bool moving_up = true;
extern absolute_time_t last_key_press_time;
int gpio_servo_control = 39; // GPIO pin connected to the servo control line

void set_servo_pwm_speed(int speed_percent) {


    uint slice_servo_num = pwm_gpio_to_slice_num(gpio_servo_control); // Assuming Pin 39 is connected to the servo control line
    // Map speed_percent (-100 to 100) to a PWM duty cycle (e.g., 1ms to 2ms pulse width for a 20ms period)
    // Assuming a 50Hz PWM frequency (20ms period), and a pulse width range of 1ms (5% duty) to 2ms (10% duty)
    int duty_cycle = (speed_percent + 100) * 5 / 200 + 5; // Maps -100 to 0% and +100 to 10%
    pwm_hw->slice[slice_servo_num].div = 25000; // Set a high divider to get a low frequency of 50Hz (assuming 125 MHz clock)
    // Set the PWM duty cycle for the servo control pin
    pwm_set_chan_level(slice_servo_num, pwm_gpio_to_channel(gpio_servo_control), duty_cycle * pwm_hw->slice[slice_servo_num].top / 100);
    pwm_set_enabled(slice_servo_num, true);
}

void run_continuous_servo_sweep() {
    // Check our sweep timer flag
    if (absolute_time_diff_us(last_servo_tick, get_absolute_time()) >= 1000000) {
        last_servo_tick = get_absolute_time();
        
        servo_runtime_seconds++;

        if (moving_up) {
            set_servo_pwm_speed(20); // Spin slowly clockwise (upward)
            
            if (servo_runtime_seconds >= 5) { // It takes 5 seconds to reach the top
                moving_up = false;
                servo_runtime_seconds = 0; // Reset timer for the way down
            }
        } else {
            set_servo_pwm_speed(-20); // Spin slowly counter-clockwise (downward)
            
            if (servo_runtime_seconds >= 5) {
                moving_up = true;
                servo_runtime_seconds = 0;
            }
        }
    }
}
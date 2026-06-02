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
int gpio_servo_control = 39; // GPIO pin connected to the servo control line

void set_servo_pwm_speed(int speed_percent) {


    uint slice_servo_num = pwm_gpio_to_slice_num(gpio_servo_control); // Assuming Pin 39 is connected to the servo control line
    // Map speed_percent (-100 to 100) to a PWM duty cycle (e.g., 1ms to 2ms pulse width for a 20ms period)
    // Assuming a 50Hz PWM frequency (20ms period), and a pulse width range of 1ms (5% duty) to 2ms (10% duty)

    
    pwm_hw->slice[slice_servo_num].div = (125*1000000)/50; // Set a high divider to get a low frequency of 50Hz (assuming 125 MHz clock)
    // Set the PWM duty cycle for the servo control pin
    pwm_set_chan_level(slice_servo_num, pwm_gpio_to_channel(gpio_servo_control), ((servo_duty_cycle/100) * pwm_hw->slice[slice_servo_num].top) / 100);
}

void init_servo_position() {
    // Check our sweep timer flag
    gpio_set_function(gpio_servo_control, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(gpio_servo_control);

    // 2. Set the frequency (Wrap value)
    // 125MHz / 12500 = 10kHz PWM frequency
    // pwm_set_wrap(slice_num, 125000); 
    
    // 3. Set initial level (50%)
    // pwm_set_chan_level(slice_num, pwm_gpio_to_channel(fan_spd_control_gpio), 62500);

    // 4. Setup Interrupts
    pwm_clear_irq(slice_num);
    pwm_set_irq0_enabled(slice_num, true);
    irq_set_exclusive_handler(PWM_IRQ_WRAP_0, set_servo_pwm_speed);
    irq_set_enabled(PWM_IRQ_WRAP_0, true);

    // 5. Start the clock
    pwm_set_enabled(slice_num, true);
}
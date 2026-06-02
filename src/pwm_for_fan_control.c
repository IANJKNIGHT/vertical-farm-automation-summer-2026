#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/irq.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"

#define NUM_SAMPLES 1000

extern uint16_t adc_fan_speed_control[NUM_SAMPLES];
extern int current_buffer;
extern int duty_cycle;
int fan_spd_control_gpio = 25;
int get_max_adc_value(uint16_t *adc_result);
int get_min_adc_value(uint16_t *adc_result);


void pwm_control()
{
    uint slice_GP35_num = pwm_gpio_to_slice_num(fan_spd_control_gpio);

    // Clear the interrupt status register flag
    pwm_hw->intr = 1u << slice_GP35_num;

    int current_period = pwm_hw->slice[slice_GP35_num].top;
    // if (current_period == 0) current_period = 125000;

    // CRITICAL FIX: Pass pwm_gpio_to_channel(35) instead of 0 to target Channel A!
    pwm_hw->slice[slice_GP35_num].div = 125000; // 1 kHz PWM frequency with 1250 divider (assuming 125 MHz clock)
    pwm_set_chan_level(slice_GP35_num, pwm_gpio_to_channel(fan_spd_control_gpio), duty_cycle * current_period / 100);
}

void init_pwm_irq() {
    // 1. Setup the physical pin
    gpio_set_function(fan_spd_control_gpio, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(fan_spd_control_gpio);

    // 2. Set the frequency (Wrap value)
    // 125MHz / 12500 = 10kHz PWM frequency
    pwm_set_wrap(slice_num, 125000); 
    
    // 3. Set initial level (50%)
    // pwm_set_chan_level(slice_num, pwm_gpio_to_channel(fan_spd_control_gpio), 62500);

    // 4. Setup Interrupts
    pwm_clear_irq(slice_num);
    pwm_set_irq0_enabled(slice_num, true);
    irq_set_exclusive_handler(PWM_IRQ_WRAP_0, pwm_control);
    irq_set_enabled(PWM_IRQ_WRAP_0, true);

    // 5. Start the clock
    pwm_set_enabled(slice_num, true);
}

void measure_duty_cycle_period(uint16_t *adc_result, int *duty_period)
{
    // fill in
    int max = get_max_adc_value(adc_result);
    int min = get_min_adc_value(adc_result);
    int average = (max + min) / 2;
    int num_rising_edges = 0;
    int first_rising_edge_index = -1;
    int second_rising_edge_index = -1;
    int is_high = 0;
    int is_low = 0;
    int high_time = 0;
    int low_time = 0;
    // calculate duty cycle based on max and min values
    for (int i = 0; i < NUM_SAMPLES; i++)
    {
        if (adc_result[i] < average)
        {
            if (i != NUM_SAMPLES - 1 && adc_result[i + 1] >= average)
            {
                // first rising edge detected
                is_high = 1;
                is_low = 0;
                num_rising_edges++;
                if (num_rising_edges == 1)
                {
                    first_rising_edge_index = i + 1;
                }
                else if (num_rising_edges == 2)
                {
                    second_rising_edge_index = i + 1;
                    break;
                }
            }
        }
        else if (adc_result[i] >= average)
        {
            if (i != NUM_SAMPLES - 1 && adc_result[i + 1] < average)
            {
                // falling edge detected
                is_low = 1;
                is_high = 0;
            }
        }
        if (first_rising_edge_index != -1 && second_rising_edge_index == -1)
        {
            if (is_high)
            {
                high_time++;
            }
            else if (is_low)
            {
                low_time++;
            }
        }
    }
    if (high_time + low_time == 0)
    {
        duty_period[0] = 0;
        duty_period[1] = 0;
        return;
    }
    duty_period[0] = high_time * 100 / (high_time + low_time);
    duty_period[1] = high_time + low_time;
    return;
}
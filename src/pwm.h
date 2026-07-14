#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/irq.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"
#include <hardware/pwm.h>

#define NUM_SAMPLES 1000

extern uint16_t adc_fan_speed_control[NUM_SAMPLES];
extern int current_buffer;
extern int duty_cycle;

extern int servo_duty_cycle;
extern int fan0_spd_control_gpio;
extern int fan1_spd_control_gpio;
extern int fan2_spd_control_gpio;

void init_all_system_pwms();
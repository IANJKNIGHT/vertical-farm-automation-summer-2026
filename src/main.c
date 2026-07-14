#include "main.h"

int fan0_spd_control_gpio = 18;
int fan1_spd_control_gpio = 19;
int fan2_spd_control_gpio = 20;
int fan0_duty_cycle;
int fan1_duty_cycle;
int fan2_duty_cycle;

int main()
{
    stdio_init_all();
    sleep_ms(3000);
    init_all_system_pwms();
    for (;;)
    {
        fan0_duty_cycle = 500;
        fan1_duty_cycle = 500;
        fan2_duty_cycle = 500;
        pwm_set_gpio_level(fan0_spd_control_gpio, fan0_duty_cycle);
        pwm_set_gpio_level(fan1_spd_control_gpio, fan1_duty_cycle);
        pwm_set_gpio_level(fan2_spd_control_gpio, fan2_duty_cycle);
        fflush(stdout);
    }
    return 0;
}
#include "pwm.h"

void init_all_system_pwms()
{
    // Target 1: Fan Speed Control on GPIO 18 (Slice 4, Chan B)
    for (int i = fan0_spd_control_gpio; i <= fan2_spd_control_gpio; i++) {
        gpio_set_function(i, GPIO_FUNC_PWM);
        uint fan_slice = pwm_gpio_to_slice_num(i);

        // ---- CONFIGURE FAN (1 kHz Base Frequency) ----
        pwm_config fan_config = pwm_get_default_config();
        pwm_config_set_clkdiv(&fan_config, 150.0f); // 1 MHz tick rate
        pwm_config_set_wrap(&fan_config, 999);      // 1000 tick period = 1 kHz
        pwm_init(fan_slice, &fan_config, true);
    }
}

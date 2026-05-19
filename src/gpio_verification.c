#include <stdio.h>
#include "pico/stdlib.h"

extern enum SystemTimerState system_timer_state;
extern enum EnterState enter_state;
extern char key_char;
int col_gpio_trigger_for_manual_mode = 6;
int row_gpio_trigger_for_enter_time = 2;

int col_gpio_first_num_col = 9;
int col_gpio_second_num_col = 8;
int col_gpio_third_num_col = 7;

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
enum EnterState
{
    FIRST_PRESS,
    SECOND_PRESS
};


void state_machine_handler()
{
    uint32_t manual_result = gpio_get_irq_event_mask(col_gpio_trigger_for_manual_mode);
    if ((manual_result & GPIO_IRQ_EDGE_RISE) && (system_timer_state == MODE_MANUAL_ADJUST))
    {
        // uint32_t gpio_state = sio_hw->gpio_in;
        uint32_t led_mask = ((1 << 22) | (1 << 23) | (1 << 24) | (1 << 25));
        // int32_t mask = 1u << (led_mask & 0x1fu);
        sio_hw->gpio_set = led_mask;
        // sio_hw->gpio_clr = led_mask; // Clear all LEDs
        // sio_hw->gpio_out |= (gpio_state & led_mask); // Set LEDs according to GPIO state
        // sio_hw->gpio_oe_set = led_mask;
        // sio_hw->gpio_clr = led_mask;
    }
    // if enter state is first_press, blink the LED once 
    if ((gpio_get_irq_event_mask(row_gpio_trigger_for_enter_time) & GPIO_IRQ_LEVEL_HIGH) && (key_char == '#'))
    {
        switch(system_timer_state)
        {
            case ENTER_DAYS:
                sio_hw->gpio_set = (1 << 22); // Turn on LED for ENTER_DAYS
                break;
            case ENTER_HOURS:
                sio_hw->gpio_set = (1 << 23); // Turn on LED for ENTER_HOURS
                break;
            case ENTER_MINUTES:
                sio_hw->gpio_set = (1 << 24); // Turn on LED for ENTER_MINUTES
                break;
            case ENTER_SECONDS:
                sio_hw->gpio_set = (1 << 25); // Turn on LED for ENTER_SECONDS
                break;
        }
    }
    
    gpio_acknowledge_irq(col_gpio_trigger_for_manual_mode, GPIO_IRQ_EDGE_RISE);
    gpio_acknowledge_irq(row_gpio_trigger_for_enter_time, GPIO_IRQ_LEVEL_HIGH);
}

void init_state_machine_led()
{
    for (int gpio = 22; gpio <= 25; gpio++)
    {
        uint32_t mask = 1u << (gpio & 0x1fu);
        sio_hw->gpio_oe_set = mask;
        sio_hw->gpio_clr = mask;

        hw_write_masked(&pads_bank0_hw->io[gpio],
                        PADS_BANK0_GPIO0_IE_BITS,
                        PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO0_OD_BITS);
        io_bank0_hw->io[gpio].ctrl = GPIO_FUNC_SIO << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;
        hw_clear_bits(&pads_bank0_hw->io[gpio], PADS_BANK0_GPIO0_ISO_BITS);
    }
    // Set the interrupt handler for GPIO 22-25 to manual_mode_adjust_led_handler
    // Set the interrupt handler for our source trigger pin
    gpio_add_raw_irq_handler_masked(1u << col_gpio_trigger_for_manual_mode, state_machine_handler);
    gpio_add_raw_irq_handler_masked(1u << row_gpio_trigger_for_enter_time, state_machine_handler);

    // Acknowledge any boot-up noise on the trigger pin safely
    gpio_acknowledge_irq(col_gpio_trigger_for_manual_mode, GPIO_IRQ_EDGE_RISE);
    gpio_acknowledge_irq(row_gpio_trigger_for_enter_time, GPIO_IRQ_LEVEL_HIGH);

    // Turn on global IO interrupts and enable the specific pin edge
    irq_set_enabled(IO_IRQ_BANK0, true);
    gpio_set_irq_enabled(col_gpio_trigger_for_manual_mode, GPIO_IRQ_EDGE_RISE, true);
    gpio_set_irq_enabled(row_gpio_trigger_for_enter_time, GPIO_IRQ_LEVEL_HIGH, true);
}

void set_time_handler()
{
    if ((gpio_get_irq_event_mask(col_gpio_first_num_col) & GPIO_IRQ_EDGE_RISE) && (system_timer_state == ENTER_DAYS))
    {
        enter_state = FIRST_PRESS;
    }
    else if ((gpio_get_irq_event_mask(col_gpio_second_num_col) & GPIO_IRQ_EDGE_RISE) && (system_timer_state == ENTER_DAYS))
    {
        enter_state = SECOND_PRESS;
    }
    else if ((gpio_get_irq_event_mask(col_gpio_third_num_col) & GPIO_IRQ_EDGE_RISE) && (system_timer_state == ENTER_DAYS))
    {
        // Handle third column press if needed
    }
}

void init_set_time()
{
    gpio_add_raw_irq_handler_masked(1u << col_gpio_first_num_col, set_time_handler);
    gpio_add_raw_irq_handler_masked(1u << col_gpio_second_num_col, set_time_handler);
    gpio_add_raw_irq_handler_masked(1u << col_gpio_third_num_col, set_time_handler);

    gpio_acknowledge_irq(col_gpio_first_num_col, GPIO_IRQ_EDGE_RISE);
    gpio_acknowledge_irq(col_gpio_second_num_col, GPIO_IRQ_EDGE_RISE);
    gpio_acknowledge_irq(col_gpio_third_num_col, GPIO_IRQ_EDGE_RISE);

    gpio_set_irq_enabled(col_gpio_first_num_col, GPIO_IRQ_EDGE_RISE, true);
    gpio_set_irq_enabled(col_gpio_second_num_col, GPIO_IRQ_EDGE_RISE, true);
    gpio_set_irq_enabled(col_gpio_third_num_col, GPIO_IRQ_EDGE_RISE, true);
    irq_set_enabled(IO_IRQ_BANK0, true);
}
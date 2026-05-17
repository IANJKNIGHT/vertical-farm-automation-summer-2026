#include "hardware/timer.h"
#include "hardware/irq.h"
#include "hardware/gpio.h"
// 7-segment display message buffer
// Declared as static to limit scope to this file only.
static char msg[8] = {
    0x3F, // seven-segment value of 0
    0x06, // seven-segment value of 1
    0x5B, // seven-segment value of 2
    0x4F, // seven-segment value of 3
    0x66, // seven-segment value of 4
    0x6D, // seven-segment value of 5
    0x7D, // seven-segment value of 6
    0x07, // seven-segment value of 7
};
extern char font[];   // Font mapping for 7-segment display
static int index = 0; // Current index in the message buffer

// We provide you with this function for directly displaying characters.
// However, it can't use the decimal point, which display_print does.
void display_char_print(const char message[])
{
    for (int i = 0; i < 8; i++)
    {
        msg[i] = font[message[i] & 0xFF];
    }
}

/********************************************************* */
// Implement the functions below.

void display_init_pins()
{
    for (int gpio = 10; gpio <= 20; gpio++)
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
}

void display_isr()
{
    hw_clear_bits(&timer1_hw->intr, 1u << 0);
    // Set the value of GP20-GP10 to a new 11-bit value
    // where the provided global variable index is used to select
    // the seven-segment display to turn on, and the 8-bit value of
    // msg[index] is used to determine which segments to light up
    sio_hw->gpio_clr = 0x7FF << 10;
    sio_hw->gpio_out |= (index << 18) | (msg[index] << 10);

    index++;
    index %= 8;
    uint32_t delay_us_alarm0 = 3 * 1000;
    uint64_t target_alarm0 = timer1_hw->timerawl + delay_us_alarm0;
    timer1_hw->alarm[0] = (uint32_t)target_alarm0;
}

void display_init_timer()
{
    uint32_t delay_us_alarm0 = 3 * 1000;
    // Enable the interrupt for our alarm (the timer outputs 4 alarm irqs)
    hw_set_bits(&timer1_hw->inte, 1u << 0);
    // Set irq handler for alarm irq
    irq_set_exclusive_handler(TIMER1_IRQ_0, display_isr);
    // Enable the alarm irq
    // TIMER1
    irq_set_enabled(TIMER1_IRQ_0, true);
    uint64_t target_alarm0 = timer1_hw->timerawl + delay_us_alarm0;
    // Write the lower 32 bits of the target time to the alarm which
    timer1_hw->alarm[0] = (uint32_t)target_alarm0;
}

void display_print(const uint16_t message[])
{
    // int i;
    // for (i = 0; i < 8; i++)
    // {
    //     uint16_t msg_msb = message[i] & 1<<8;
    //     msg[i] = (msg_msb>>1) | font[(message[i]) & 0xFF];
    // }
}